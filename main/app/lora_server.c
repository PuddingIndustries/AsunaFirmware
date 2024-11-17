#include <string.h>

/* IDF */
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/list.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

/* LLCC68 */
#include "lora_modem.h"

/* App */
#include "app/gnss_server.h"
#include "app/lora_server.h"

#define APP_LORA_SERVER_NVS_NAMESPACE "a_lora_server"
#define APP_LORA_SERVER_NVS_VERSION   1 /* DO NOT CHANGE THIS VALUE UNLESS THERE IS A STRUCTURE UPDATE */

#define APP_LORA_SERVER_SPI_HOST SPI2_HOST
#define APP_LORA_SERVER_SPI_FREQ (4 * 1000 * 1000)
#define APP_LORA_SERVER_PIN_SCK  (12)
#define APP_LORA_SERVER_PIN_MOSI (11)
#define APP_LORA_SERVER_PIN_MISO (13)
#define APP_LORA_SERVER_PIN_CS   (10)
#define APP_LORA_SERVER_PIN_RST  (21)
#define APP_LORA_SERVER_PIN_INT  (9)
#define APP_LORA_SERVER_PIN_BUSY (14)

#define APP_LORA_SERVER_CMD_Q_LEN (16)

#define APP_LORA_SERVER_FREQUENCY_MIN     (868 * 1000 * 1000)     /* TODO: Use Kconfig */
#define APP_LORA_SERVER_FREQUENCY_MAX     (915 * 1000 * 1000 - 1) /* TODO: Use Kconfig */
#define APP_LORA_SERVER_FREQUENCY_DEFAULT (868400000UL)           /* 868.400 MHz */

#define APP_LORA_SERVER_POWER_DEFAULT (7) /* 7dBm */

typedef struct {
    uint8_t *data;
    size_t   data_len;
} app_lora_server_cmd_queue_item_t;

typedef struct {
    lora_modem_t         lora_modem;
    app_gnss_cb_handle_t gnss_cb_handle;

    TaskHandle_t task_broadcast;
    TaskHandle_t task_manager;

    SemaphoreHandle_t mutex_modem;

    QueueHandle_t queue_transmit;

    uint8_t *rtcm_buffer;
    size_t   rtcm_buffer_size;
} app_lora_server_state_t;

static const char *LOG_TAG = "asuna_lora";

static int  app_lora_server_gnss_forwarder_cb(void *handle, app_gnss_cb_type_t type, void *payload);
static void app_lora_server_gpio_init(void);
static int  app_lora_server_spi_init(void);
static int  app_lora_modem_ops_spi(void *handle, const lora_modem_spi_transfer_t *transfer);
static int  app_lora_modem_ops_pin(void *handle, lora_modem_pin_t pin, bool value);
static int  app_lora_modem_ops_wait_busy(void *handle);
static int  app_lora_modem_ops_delay(void *handle, uint32_t delay_ms);
static void app_lora_modem_cb_event(void *handle, lora_modem_cb_event_t event);
static void app_lora_server_irq_handler(void *arg);
static void app_lora_server_broadcast_task(void *argument);
static void app_lora_server_manager_task(void *argument);

static app_lora_server_state_t s_lora_server_state = {
    .lora_modem =
        {
            .handle = NULL,

            .ops =
                {
                    .spi       = app_lora_modem_ops_spi,
                    .pin       = app_lora_modem_ops_pin,
                    .wait_busy = app_lora_modem_ops_wait_busy,
                    .delay     = app_lora_modem_ops_delay,
                },

            .cb = app_lora_modem_cb_event,
        },
    .gnss_cb_handle = NULL,
    .task_broadcast = NULL,
    .task_manager   = NULL,
    .mutex_modem    = NULL,
    .queue_transmit = NULL,

    .rtcm_buffer      = NULL,
    .rtcm_buffer_size = 0,
};

static const char *APP_LORA_SERVER_CFG_KEY_FLAG    = "cfg_valid"; /* Configuration key */
static const char *APP_LORA_SERVER_CFG_KEY_FW_RTCM = "fw_rtcm";   /* Forward RTCM payload */
static const char *APP_LORA_SERVER_CFG_KEY_FREQ    = "freq";      /* Frequency */
static const char *APP_LORA_SERVER_CFG_KEY_POWER   = "power";     /* Transmit Power */
static const char *APP_LORA_SERVER_CFG_KEY_TYPE    = "type";      /* Network Type */
static const char *APP_LORA_SERVER_CFG_KEY_BW      = "bw";        /* Bandwidth */
static const char *APP_LORA_SERVER_CFG_KEY_SF      = "sf";        /* Spreading Factor */
static const char *APP_LORA_SERVER_CFG_KEY_CR      = "cr";        /* Coding Rate */
static const char *APP_LORA_SERVER_CFG_KEY_LDR_OPT = "ldr_opt";   /* Low Data-Rate Optimization */

int app_lora_server_init(void) {
    int ret = 0;

    ESP_LOGI(LOG_TAG, "Initializing LoRa server...");

    s_lora_server_state.mutex_modem = xSemaphoreCreateRecursiveMutex();
    if (s_lora_server_state.mutex_modem == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create mutex.");
        return -3;
    }

    s_lora_server_state.queue_transmit =
        xQueueCreate(APP_LORA_SERVER_CMD_Q_LEN, sizeof(app_lora_server_cmd_queue_item_t));
    if (s_lora_server_state.queue_transmit == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create command queue.");

        ret = -4;
        goto del_mutex_exit;
    }

    app_lora_server_gpio_init();

    if (app_lora_server_spi_init() != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize SPI interface.");
        goto del_queue_exit;
    }

    if (xSemaphoreTakeRecursive(s_lora_server_state.mutex_modem, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to acquire lock.");
        goto del_queue_exit;
    }

    ret = lora_modem_init(&s_lora_server_state.lora_modem);
    if (ret != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize LoRa modem: %d", ret);
        goto del_queue_exit;
    }

    app_lora_server_config_t cfg;

    if (app_lora_server_config_get(&cfg) != 0) {
        ESP_LOGW(LOG_TAG, "Configuration invalid, restore to default...");
        app_lora_server_config_init(&cfg);

        if (app_lora_server_config_set(&cfg) != 0) {
            ESP_LOGE(LOG_TAG, "Configuration validation failed...");
            goto del_queue_exit;
        }
    } else {
        ret = lora_modem_set_config(&s_lora_server_state.lora_modem, &cfg.modem_config);
        if (ret != 0) {
            goto del_queue_exit;
        }
    }

    xSemaphoreGiveRecursive(s_lora_server_state.mutex_modem);

    if (xTaskCreate(app_lora_server_manager_task, "asuna_lrm", 3072, &s_lora_server_state, 4,
                    &s_lora_server_state.task_manager) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Manager task creation failed...");
        ret = -3;

        goto del_queue_exit;
    }

    return 0;

del_queue_exit:
    vQueueDelete(s_lora_server_state.queue_transmit);

del_mutex_exit:
    vSemaphoreDelete(s_lora_server_state.mutex_modem);

    return ret;
}

void app_lora_server_config_init(app_lora_server_config_t *config) {
    config->fw_rtcm = false;

    config->modem_config.frequency        = APP_LORA_SERVER_FREQUENCY_DEFAULT;
    config->modem_config.power            = APP_LORA_SERVER_POWER_DEFAULT;
    config->modem_config.network_type     = LORA_MODEM_NETWORK_PRIVATE;
    config->modem_config.bandwidth        = LORA_MODEM_BW_125;
    config->modem_config.coding_rate      = LORA_MODEM_CR_1;
    config->modem_config.spreading_factor = LORA_MODEM_SF_7;
    config->modem_config.ldr_optimization = false;
}

int app_lora_server_config_set(const app_lora_server_config_t *config) {
    /* ---- Sanity checks ---- */
    if (config->modem_config.bandwidth >= LORA_MODEM_BW_INVALID) return -1;
    if (config->modem_config.coding_rate >= LORA_MODEM_CR_INVALID) return -1;
    if (config->modem_config.spreading_factor >= LORA_MODEM_SF_INVALID) return -1;
    if (config->modem_config.frequency > APP_LORA_SERVER_FREQUENCY_MAX) return -1;
    if (config->modem_config.frequency < APP_LORA_SERVER_FREQUENCY_MIN) return -1;

    if (xSemaphoreTakeRecursive(s_lora_server_state.mutex_modem, portMAX_DELAY) != pdPASS) {
        return -2;
    }

    /* ---- Create NVS handle ---- */
    nvs_handle handle;
    ESP_ERROR_CHECK(nvs_open(APP_LORA_SERVER_NVS_NAMESPACE, NVS_READWRITE, &handle));

    /* ---- Store configuration ---- */
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_FLAG, APP_LORA_SERVER_NVS_VERSION));

    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_FW_RTCM, config->fw_rtcm));
    ESP_ERROR_CHECK(nvs_set_u32(handle, APP_LORA_SERVER_CFG_KEY_FREQ, config->modem_config.frequency));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_POWER, config->modem_config.power));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_TYPE, config->modem_config.network_type));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_BW, config->modem_config.bandwidth));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_CR, config->modem_config.coding_rate));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_SF, config->modem_config.spreading_factor));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_LDR_OPT, config->modem_config.ldr_optimization));

    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);

    lora_modem_set_config(&s_lora_server_state.lora_modem, &config->modem_config);
    xSemaphoreGiveRecursive(s_lora_server_state.mutex_modem);

    if (config->fw_rtcm) {
        if (s_lora_server_state.gnss_cb_handle == NULL) {
            s_lora_server_state.gnss_cb_handle = app_gnss_server_cb_register(
                APP_GNSS_CB_RAW_RTCM, app_lora_server_gnss_forwarder_cb, &s_lora_server_state);
        }
    } else {
        if (s_lora_server_state.gnss_cb_handle != NULL) {
            app_gnss_server_cb_unregister(s_lora_server_state.gnss_cb_handle);

            s_lora_server_state.gnss_cb_handle = NULL;
        }
    }

    return 0;
}

int app_lora_server_config_get(app_lora_server_config_t *config) {
    int       ret = 0;
    esp_err_t err;

    if (xSemaphoreTakeRecursive(s_lora_server_state.mutex_modem, portMAX_DELAY) != pdPASS) {
        return -1;
    }

    /* ---- Create NVS handle ---- */
    nvs_handle handle;
    err = nvs_open(APP_LORA_SERVER_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ret = -1;
        goto release_lock_exit;
    }

    /* Check NVS data flag */

    uint8_t cfg_flag;
    if (nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_FLAG, &cfg_flag) != ESP_OK) {
        ret = -1;
        goto release_lock_exit;
    }

    /* ?? Downgrade is not allowed ?? */
    if (cfg_flag > APP_LORA_SERVER_NVS_VERSION) {
        ret = -2;
        goto release_lock_exit;
    }

    /* TODO: Handle structure update. */

    /* ---- Load configuration: fw_rtcm ---- */
    uint8_t fw_rtcm;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_FW_RTCM, &fw_rtcm));
    config->fw_rtcm = fw_rtcm;

    /* ---- Load configuration: frequency ---- */
    ESP_ERROR_CHECK(nvs_get_u32(handle, APP_LORA_SERVER_CFG_KEY_FREQ, &config->modem_config.frequency));

    /* ---- Load configuration: power ---- */
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_POWER, &config->modem_config.power));

    /* ---- Load configuration: network_type ---- */
    uint8_t network_type;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_TYPE, &network_type));
    config->modem_config.network_type = network_type;

    /* ---- Load configuration: bandwidth ---- */
    uint8_t bandwidth;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_BW, &bandwidth));
    config->modem_config.bandwidth = bandwidth;

    /* ---- Load configuration: coding_rate ---- */
    uint8_t coding_rate;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_CR, &coding_rate));
    config->modem_config.coding_rate = coding_rate;

    /* ---- Load configuration: spreading_factor ---- */
    uint8_t spreading_factor;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_SF, &spreading_factor));
    config->modem_config.spreading_factor = spreading_factor;

    /* ---- Load configuration: ldr_optimization ---- */
    uint8_t ldr_optimization;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_LDR_OPT, &ldr_optimization));
    config->modem_config.ldr_optimization = ldr_optimization;

    /* ---- Close NVS handle ---- */
    nvs_close(handle);

release_lock_exit:
    xSemaphoreGiveRecursive(s_lora_server_state.mutex_modem);

    return ret;
}

int app_lora_server_broadcast(const uint8_t *data, size_t length) {
    void *payload = malloc(length);
    if (payload == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to allocate packet buffer");

        return -1;
    }

    memcpy(payload, data, length);

    const app_lora_server_cmd_queue_item_t cmd = {
        .data     = payload,
        .data_len = length,
    };

    if (xQueueSend(s_lora_server_state.queue_transmit, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(LOG_TAG, "Failed to enqueue packet.");
        goto free_buf_exit;
    }

    return 0;

free_buf_exit:
    free(payload);

    return -1;
}

static int app_lora_server_gnss_forwarder_cb(void *handle, app_gnss_cb_type_t type, void *payload) {
    app_lora_server_state_t *state = handle;

    if (type == APP_GNSS_CB_RAW_RTCM) {
        const app_gnss_rtcm_t *data = payload;

        uint8_t *new_buffer = realloc(state->rtcm_buffer, (state->rtcm_buffer_size + data->data_len));
        if (new_buffer == NULL) {
            ESP_LOGE(LOG_TAG, "Failed to allocate buffer for RTCM payload.");
            free(state->rtcm_buffer);

            return -1;
        }

        state->rtcm_buffer = new_buffer;

        memcpy(&state->rtcm_buffer[state->rtcm_buffer_size], data->data, data->data_len);

        state->rtcm_buffer_size += data->data_len;

        if (state->rtcm_buffer_size >= 256) {
            app_lora_server_broadcast(state->rtcm_buffer, state->rtcm_buffer_size);

            free(state->rtcm_buffer);

            state->rtcm_buffer      = NULL;
            state->rtcm_buffer_size = 0;
        }
    }

    return 0;
}

static void app_lora_server_gpio_init(void) {
    gpio_config_t pin_cfg = {
        .pin_bit_mask = BIT64(APP_LORA_SERVER_PIN_CS) | BIT64(APP_LORA_SERVER_PIN_RST),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    gpio_config(&pin_cfg);

    pin_cfg.pin_bit_mask = BIT64(APP_LORA_SERVER_PIN_BUSY);
    pin_cfg.mode         = GPIO_MODE_INPUT;
    pin_cfg.pull_up_en   = GPIO_PULLUP_ENABLE;

    gpio_config(&pin_cfg);

    pin_cfg.pin_bit_mask = BIT64(APP_LORA_SERVER_PIN_INT);
    pin_cfg.mode         = GPIO_MODE_INPUT;
    pin_cfg.intr_type    = GPIO_INTR_POSEDGE;

    gpio_config(&pin_cfg);

    gpio_set_level(APP_LORA_SERVER_PIN_CS, 1U);
    gpio_set_level(APP_LORA_SERVER_PIN_RST, 1U);
}

static int app_lora_server_spi_init(void) {
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num     = APP_LORA_SERVER_PIN_MOSI,
        .miso_io_num     = APP_LORA_SERVER_PIN_MISO,
        .sclk_io_num     = APP_LORA_SERVER_PIN_SCK,
        .quadhd_io_num   = -1,
        .quadwp_io_num   = -1,
        .max_transfer_sz = 256,
    };

    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = APP_LORA_SERVER_SPI_FREQ,
        .mode           = 0,
        .spics_io_num   = -1,
        .queue_size     = 2,
    };

    if (spi_bus_initialize(APP_LORA_SERVER_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return -1;
    }

    spi_device_handle_t spi_device;

    if (spi_bus_add_device(APP_LORA_SERVER_SPI_HOST, &dev_cfg, &spi_device) != ESP_OK) {
        return -2;
    }

    s_lora_server_state.lora_modem.handle = spi_device;

    return 0;
}

static int app_lora_modem_ops_spi(void *handle, const lora_modem_spi_transfer_t *transfer) {
    spi_device_handle_t spi_device = handle;

    spi_transaction_t txn = {
        .tx_buffer = transfer->tx_data,
        .rx_buffer = transfer->rx_data,
        .length    = transfer->length * 8,
    };

    if (spi_device_polling_transmit(spi_device, &txn) != ESP_OK) {
        ESP_LOGE(LOG_TAG, "SPI transaction failed, len: %d", txn.length);

        return -1;
    }

    return 0;
}

static int app_lora_modem_ops_pin(void *handle, lora_modem_pin_t pin, bool value) {
    gpio_num_t pin_num;

    switch (pin) {
        case LORA_MODEM_PIN_CS:
            pin_num = APP_LORA_SERVER_PIN_CS;
            break;
        case LORA_MODEM_PIN_RESET:
            pin_num = APP_LORA_SERVER_PIN_RST;
            break;

        default:
            return -1;
    }

    gpio_set_level(pin_num, value);

    return 0;
}

static int app_lora_modem_ops_wait_busy(void *handle) {
    while (true) {
        const int busy = gpio_get_level(APP_LORA_SERVER_PIN_BUSY);
        if (!busy) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    return 0;
}

static int app_lora_modem_ops_delay(void *handle, const uint32_t delay_ms) {
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return 0;
}

static void app_lora_modem_cb_event(void *handle, const lora_modem_cb_event_t event) {
    switch (event) {
        case LORA_MODEM_CB_EVENT_TX_DONE: {
            ESP_LOGD(LOG_TAG, "Received TX_DONE event.");

            xTaskNotify(s_lora_server_state.task_broadcast, BIT(0), eSetBits);

            break;
        }

        default:
            break;
    }
}

static void app_lora_server_irq_handler(void *arg) {
    BaseType_t higher_priority_task_woken = pdFALSE;

    xTaskNotifyFromISR(s_lora_server_state.task_manager, BIT(0), eSetBits, &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void app_lora_server_broadcast_task(void *argument) {
    const lora_modem_t *modem = &((app_lora_server_state_t *)argument)->lora_modem;

    app_lora_server_cmd_queue_item_t cmd;
    uint32_t                         notified_value;

    for (;;) {
        if (xQueueReceive(s_lora_server_state.queue_transmit, &cmd, portMAX_DELAY) != pdPASS) {
            ESP_LOGW(LOG_TAG, "Failed to receive from queue.");
            continue;
        }

        /* Split stream longer than 256 bytes into multiple packets. */

        size_t data_ptr = 0;
        while (data_ptr < cmd.data_len) {
            size_t btw = cmd.data_len - data_ptr;
            if (btw > 256) btw = 256;

            ESP_LOGD(LOG_TAG, "Transmitting %d of %d packet.", data_ptr + btw, cmd.data_len);

            if (xSemaphoreTakeRecursive(s_lora_server_state.mutex_modem, portMAX_DELAY) != pdPASS) {
                ESP_LOGE(LOG_TAG, "Failed to acquire lock.");
                continue;
            }

            const int ret = lora_modem_transmit(modem, &cmd.data[data_ptr], btw);

            xSemaphoreGiveRecursive(s_lora_server_state.mutex_modem);

            data_ptr += btw;

            if (ret != 0) {
                ESP_LOGW(LOG_TAG, "Failed to transmit data.");
            }

            if (xTaskNotifyWait(0UL, 0xFFFFFFFFUL, &notified_value, portMAX_DELAY) != pdPASS) {
                ESP_LOGW(LOG_TAG, "Failed to wait for TX_DONE signal.");
            }
        }

        free(cmd.data);
    }
}

static void app_lora_server_manager_task(void *argument) {
    const lora_modem_t *modem = &((app_lora_server_state_t *)argument)->lora_modem;

    uint32_t notified_value;

    gpio_isr_handler_add(APP_LORA_SERVER_PIN_INT, app_lora_server_irq_handler, NULL);

    if (xTaskCreate(app_lora_server_broadcast_task, "asuna_lrb", 3072, &s_lora_server_state, 3,
                    &s_lora_server_state.task_broadcast) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Task creation failed...");
        vTaskDelete(NULL);
    }

    for (;;) {
        if (xTaskNotifyWait(0UL, 0xFFFFFFFFUL, &notified_value, portMAX_DELAY) != pdPASS) {
            ESP_LOGW(LOG_TAG, "Failed to wait for signals.");
        }

        if (xSemaphoreTakeRecursive(s_lora_server_state.mutex_modem, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(LOG_TAG, "Failed to acquire lock.");
            continue;
        }

        lora_modem_handle_interrupt(modem);

        xSemaphoreGiveRecursive(s_lora_server_state.mutex_modem);
    }

    for (;;) {
    }
}
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
#include "llcc68.h"
#include "llcc68_hal_init.h"

/* App */
#include "app/lora_server.h"

#define APP_LORA_SERVER_NVS_NAMESPACE "a_lora_server"
#define APP_LORA_SERVER_NVS_VERSION   1 /* DO NOT CHANGE THIS VALUE UNLESS THERE IS A STRUCTURE UPDATE */

#define APP_LORA_SERVER_SPI_HOST SPI2_HOST
#define APP_LORA_SERVER_SPI_FREQ (4 * 1000 * 1000)
#define APP_LORA_SERVER_PIN_SCK  12
#define APP_LORA_SERVER_PIN_MOSI 11
#define APP_LORA_SERVER_PIN_MISO 13
#define APP_LORA_SERVER_PIN_CS   10
#define APP_LORA_SERVER_PIN_RST  21
#define APP_LORA_SERVER_PIN_INT  9
#define APP_LORA_SERVER_PIN_BUSY 14

#define APP_LORA_SERVER_FREQUENCY_MIN (470 * 1000 * 1000)     /* TODO: Use Kconfig */
#define APP_LORA_SERVER_FREQUENCY_MAX (510 * 1000 * 1000 - 1) /* TODO: Use Kconfig */

typedef struct {
    llcc68_hal_context_t hal_context;
    SemaphoreHandle_t    config_mutex;
} app_lora_server_state_t;

static const char *LOG_TAG = "asuna_lora";

static void                app_lora_server_gpio_init(void);
static int                 app_lora_server_spi_init(void);
static llcc68_hal_status_t app_llcc68_hal_spi_ops(void *handle, llcc68_hal_spi_transfer_t *xfer);
static llcc68_hal_status_t app_llcc68_hal_pin_ops(void *handle, llcc68_hal_pin_t pin, bool value);
static llcc68_hal_status_t app_llcc68_hal_wait_busy(void *handle);
static llcc68_hal_status_t app_llcc68_hal_delay(void *handle, uint32_t msec);
static void                app_lora_server_task(void *argument);

static app_lora_server_state_t s_lora_server_state = {
    .hal_context =
        {
            .handle = NULL,

            .spi_ops   = app_llcc68_hal_spi_ops,
            .pin_ops   = app_llcc68_hal_pin_ops,
            .wait_busy = app_llcc68_hal_wait_busy,
            .delay     = app_llcc68_hal_delay,
        },
    .config_mutex = NULL,
};

static const char *APP_NETIF_WIFI_CFG_KEY_FLAG     = "cfg_valid";
static const char *APP_LORA_SERVER_CFG_KEY_FREQ    = "freq";    /* Frequency */
static const char *APP_LORA_SERVER_CFG_KEY_BW      = "bw";      /* Bandwidth */
static const char *APP_LORA_SERVER_CFG_KEY_SF      = "sf";      /* Spreading Factor */
static const char *APP_LORA_SERVER_CFG_KEY_CR      = "cr";      /* Coding Rate */
static const char *APP_LORA_SERVER_CFG_KEY_LDR_OPT = "ldr_opt"; /* Low Data-Rate Optimization */

int app_lora_server_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing LoRa server...");

    app_lora_server_gpio_init();

    if (app_lora_server_spi_init() != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize SPI interface.");
        return -2;
    }

    s_lora_server_state.config_mutex = xSemaphoreCreateMutex();
    if (s_lora_server_state.config_mutex == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create mutex.");
        return -3;
    }

    app_lora_server_config_t app_cfg;
    if (app_lora_server_config_get(&app_cfg) != 0) {
        ESP_LOGW(LOG_TAG, "Configuration invalid, restore to default...");
        app_lora_server_config_init(&app_cfg);

        if (app_lora_server_config_set(&app_cfg) != 0) {
            ESP_LOGE(LOG_TAG, "Configuration validation failed...");
            return -4;
        }
    }

    xTaskCreate(app_lora_server_task, "asuna_lora", 4096, &s_lora_server_state, 3, NULL);

    ESP_LOGI(LOG_TAG, "Initialization completed.");

    return 0;
}

void app_lora_server_config_init(app_lora_server_config_t *config) {
    config->frequency        = 475600000; /* 475.600 MHz */
    config->bandwidth        = APP_LORA_SERVER_BW_125;
    config->coding_rate      = APP_LORA_SERVER_CR_1;
    config->spreading_factor = APP_LORA_SERVER_SF_7;
    config->ldr_optimization = false;
}

int app_lora_server_config_set(const app_lora_server_config_t *config) {
    /* ---- Sanity checks ---- */
    if (config->bandwidth >= APP_LORA_SERVER_BW_INVALID) return -1;
    if (config->coding_rate >= APP_LORA_SERVER_CR_INVALID) return -1;
    if (config->spreading_factor >= APP_LORA_SERVER_SF_INVALID) return -1;
    if (config->frequency > APP_LORA_SERVER_FREQUENCY_MAX) return -1;
    if (config->frequency < APP_LORA_SERVER_FREQUENCY_MIN) return -1;

    if (xSemaphoreTake(s_lora_server_state.config_mutex, portMAX_DELAY) != pdPASS) {
        return -2;
    }

    /* ---- Create NVS handle ---- */
    nvs_handle handle;
    ESP_ERROR_CHECK(nvs_open(APP_LORA_SERVER_NVS_NAMESPACE, NVS_READWRITE, &handle));

    /* ---- Store configuration ---- */
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_NETIF_WIFI_CFG_KEY_FLAG, APP_LORA_SERVER_NVS_VERSION));

    ESP_ERROR_CHECK(nvs_set_u32(handle, APP_LORA_SERVER_CFG_KEY_FREQ, config->frequency));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_BW, config->bandwidth));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_CR, config->coding_rate));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_SF, config->spreading_factor));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_LORA_SERVER_CFG_KEY_LDR_OPT, config->ldr_optimization));

    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);

    xSemaphoreGive(s_lora_server_state.config_mutex);

    return 0;
}

int app_lora_server_config_get(app_lora_server_config_t *config) {
    int       ret = 0;
    esp_err_t err;

    if (xSemaphoreTake(s_lora_server_state.config_mutex, portMAX_DELAY) != pdPASS) {
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
    if (nvs_get_u8(handle, APP_NETIF_WIFI_CFG_KEY_FLAG, &cfg_flag) != ESP_OK) {
        ret = -1;
        goto release_lock_exit;
    }

    /* ?? Downgrade is not allowed ?? */
    if (cfg_flag > APP_LORA_SERVER_NVS_VERSION) {
        ret = -2;
        goto release_lock_exit;
    }

    /* TODO: Handle structure update. */

    /* ---- Load configuration: frequency ---- */
    ESP_ERROR_CHECK(nvs_get_u32(handle, APP_LORA_SERVER_CFG_KEY_FREQ, &config->frequency));

    /* ---- Load configuration: bandwidth ---- */
    uint8_t bandwidth;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_BW, &bandwidth));
    config->bandwidth = bandwidth;

    /* ---- Load configuration: coding_rate ---- */
    uint8_t coding_rate;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_CR, &coding_rate));
    config->coding_rate = coding_rate;

    /* ---- Load configuration: spreading_factor ---- */
    uint8_t spreading_factor;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_SF, &spreading_factor));
    config->spreading_factor = spreading_factor;

    /* ---- Load configuration: ldr_optimization ---- */
    uint8_t ldr_optimization;
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_LORA_SERVER_CFG_KEY_LDR_OPT, &ldr_optimization));
    config->ldr_optimization = ldr_optimization;

    /* ---- Close NVS handle ---- */
    nvs_close(handle);

release_lock_exit:
    xSemaphoreGive(s_lora_server_state.config_mutex);

    return ret;
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

    s_lora_server_state.hal_context.handle = spi_device;

    return 0;
}

static llcc68_hal_status_t app_llcc68_hal_spi_ops(void *handle, llcc68_hal_spi_transfer_t *xfer) {
    spi_device_handle_t spi_device = handle;

    spi_transaction_t txn = {
        .tx_buffer = xfer->tx_data,
        .rx_buffer = xfer->rx_data,
        .length    = xfer->length * 8,
    };

    if (spi_device_polling_transmit(spi_device, &txn) != ESP_OK) {
        ESP_LOGE(LOG_TAG, "SPI transaction failed, len: %d", txn.length);

        return LLCC68_HAL_STATUS_ERROR;
    }

    return LLCC68_HAL_STATUS_OK;
}

static llcc68_hal_status_t app_llcc68_hal_pin_ops(void *handle, llcc68_hal_pin_t pin, bool value) {
    gpio_num_t pin_num;

    switch (pin) {
        case LLCC68_HAL_PIN_CS:
            pin_num = APP_LORA_SERVER_PIN_CS;
            break;
        case LLCC68_HAL_PIN_RESET:
            pin_num = APP_LORA_SERVER_PIN_RST;
            break;

        default:
            return LLCC68_HAL_STATUS_ERROR;
    }

    gpio_set_level(pin_num, value);

    return LLCC68_HAL_STATUS_OK;
}

static llcc68_hal_status_t app_llcc68_hal_wait_busy(void *handle) {
    while (true) {
        const int busy = gpio_get_level(APP_LORA_SERVER_PIN_BUSY);
        if (!busy) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return LLCC68_HAL_STATUS_OK;
}

static llcc68_hal_status_t app_llcc68_hal_delay(void *handle, uint32_t msec) {
    vTaskDelay(pdMS_TO_TICKS(msec));
    return LLCC68_HAL_STATUS_OK;
}

static int app_lora_server_comms_init(void *context) {
    llcc68_status_t status = LLCC68_STATUS_ERROR;

    status = llcc68_reset(context);
    if (status != LLCC68_STATUS_OK) {
        return -1;
    }

    status = llcc68_init_retention_list(context);
    if (status != LLCC68_STATUS_OK) {
        return -2;
    }

    status = llcc68_set_reg_mode(context, LLCC68_REG_MODE_DCDC);
    if (status != LLCC68_STATUS_OK) {
        return -3;
    }

    status = llcc68_set_dio2_as_rf_sw_ctrl(context, true);
    if (status != LLCC68_STATUS_OK) {
        return -4;
    }

    status = llcc68_set_dio3_as_tcxo_ctrl(context, LLCC68_TCXO_CTRL_3_3V, 500);
    if (status != LLCC68_STATUS_OK) {
        return -5;
    }

    status = llcc68_cal_img_in_mhz(context, 470, 510);
    if (status != LLCC68_STATUS_OK) {
        return -6;
    }

    return 0;
}

static void app_lora_server_task(void *argument) {
    void *context = &((app_lora_server_state_t *)argument)->hal_context;

    bool is_initialized = false;

    for (;;) {
        if (!is_initialized) {
            int ret = app_lora_server_comms_init(context);
            if (ret != 0) {
                ESP_LOGE(LOG_TAG, "Failed to initialize LoRa modem: %d", ret);

                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            is_initialized = true;
            ESP_LOGI(LOG_TAG, "LoRa modem initialized.");
        }

        /* TODO: Handle device requests. */
        vTaskSuspend(NULL);
    }
}
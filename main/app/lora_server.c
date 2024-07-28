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

/* LLCC68 */
#include "llcc68.h"
#include "llcc68_hal_init.h"

/* App */
#include "app/lora_server.h"

static const char *LOG_TAG = "asuna_lora";

#define APP_LORA_SERVER_SPI_HOST SPI2_HOST
#define APP_LORA_SERVER_SPI_FREQ (4 * 1000 * 1000)
#define APP_LORA_SERVER_PIN_SCK  35
#define APP_LORA_SERVER_PIN_MOSI 36
#define APP_LORA_SERVER_PIN_MISO 37
#define APP_LORA_SERVER_PIN_CS   14
#define APP_LORA_SERVER_PIN_RST  40
#define APP_LORA_SERVER_PIN_BUSY 13

static llcc68_hal_status_t app_llcc68_hal_spi_ops(void *handle, llcc68_hal_spi_transfer_t *xfer);
static llcc68_hal_status_t app_llcc68_hal_pin_ops(void *handle, llcc68_hal_pin_t pin, bool value);
static llcc68_hal_status_t app_llcc68_hal_wait_busy(void *handle);
static llcc68_hal_status_t app_llcc68_hal_delay(void *handle, uint32_t msec);
static void                app_lora_server_task(void *argument);

static llcc68_hal_context_t s_app_lora_context = {
    .handle = NULL,

    .spi_ops   = app_llcc68_hal_spi_ops,
    .pin_ops   = app_llcc68_hal_pin_ops,
    .wait_busy = app_llcc68_hal_wait_busy,
    .delay     = app_llcc68_hal_delay,
};

int app_lora_server_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing LoRa server...");

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

    s_app_lora_context.handle = spi_device;

    xTaskCreate(app_lora_server_task, "A_LORA", 2048, &s_app_lora_context, 3, NULL);

    ESP_LOGI(LOG_TAG, "Initialization completed.");

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

    return 0;
}

static void app_lora_server_task(void *argument) {
    void *context = argument;

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
#include <string.h>

/* IDF */
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* App */
#include "app/gnss_server.h"

/* nl */
#include "rcv/nl_rtcm3.h"
#include "rcv/nmea.h"

#define GNSS_UART_NUM      UART_NUM_2
#define GNSS_UART_BUF_SIZE (2048)
#define GNSS_TX_PIN        GPIO_NUM_1  // TODO: Use Kconfig
#define GNSS_RX_PIN        GPIO_NUM_2  // TODO: Use Kconfig

typedef struct {
    QueueHandle_t uart_rx_queue;
    TaskHandle_t  uart_rx_task;

    nmea_raw_t nmea_raw;
    nl_rtcm_t  rtcm_raw;
} app_gnss_server_ctx_t;

static const char* LOG_TAG = "asuna_gnss";

static void app_gnss_uart_event_task(void* parameters);

int app_gnss_server_init(void) {
    app_gnss_server_ctx_t* ctx = malloc(sizeof(app_gnss_server_ctx_t));
    if (ctx == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to allocate GNSS context.");

        return -1;
    }

    memset(ctx, 0x00U, sizeof(app_gnss_server_ctx_t));

    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_set_pin(GNSS_UART_NUM, GNSS_TX_PIN, GNSS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GNSS_UART_NUM, GNSS_UART_BUF_SIZE, GNSS_UART_BUF_SIZE, 8, &ctx->uart_rx_queue, 0);
    uart_param_config(GNSS_UART_NUM, &uart_config);

    if (xTaskCreate(app_gnss_uart_event_task, "A_GNSS", 4096, ctx, 5, &ctx->uart_rx_task) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create GNSS UART event task.");
        return -1;
    }

    ESP_LOGI(LOG_TAG, "GNSS server initialization completed.");
    return 0;
}

static void app_gnss_uart_event_task(void* parameters) {
    int                    i;
    app_gnss_server_ctx_t* ctx = parameters;
    uart_event_t           event;
    uint8_t                dtmp[GNSS_UART_BUF_SIZE];

    for (;;) {
        if (xQueueReceive(ctx->uart_rx_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA: {
                    uart_read_bytes(GNSS_UART_NUM, dtmp, event.size, portMAX_DELAY);

                    for (i = 0; i < event.size; i++) {
                        if (input_nmea(&ctx->nmea_raw, dtmp[i])) {
                            ESP_LOGI(LOG_TAG, "NMEA[%c%c%c] received", ctx->nmea_raw.type[0], ctx->nmea_raw.type[1],
                                     ctx->nmea_raw.type[2]);
                        }

                        if (nl_input_rtcm3_v2(&ctx->rtcm_raw, dtmp[i])) {
                            ESP_LOGI(LOG_TAG, "RTCM[%d] received", ctx->rtcm_raw.type);
                        }
                    }
                    break;
                }

                case UART_FIFO_OVF: {
                    ESP_LOGE(LOG_TAG, "Hardware FIFO overflowed...");
                    uart_flush_input(GNSS_UART_NUM);

                    xQueueReset(ctx->uart_rx_queue);
                    break;
                }

                default:
                    ESP_LOGW(LOG_TAG, "Unhandled UART event type: %d", event.type);
                    break;
            }
        }
    }
}

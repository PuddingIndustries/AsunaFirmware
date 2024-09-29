#include <string.h>

/* IDF */
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/list.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* App */
#include "app/gnss_server.h"

/* nl */
#include "rcv/nl_rtcm3.h"
#include "rcv/nmea.h"

#define GNSS_UART_NUM      UART_NUM_2
#define GNSS_UART_BUF_SIZE (2048)
#define GNSS_TX_PIN        CONFIG_APP_GNSS_SERVER_TX_GPIO
#define GNSS_RX_PIN        CONFIG_APP_GNSS_SERVER_RX_GPIO
#define GNSS_RST_PIN       CONFIG_APP_GNSS_SERVER_RST_GPIO

typedef struct {
    QueueHandle_t uart_rx_queue;
    TaskHandle_t  uart_rx_task;

    nmea_raw_t nmea_raw;
    nl_rtcm_t  rtcm_raw;
} app_gnss_server_ctx_t;

typedef struct {
    app_gnss_cb_type_t type;
    app_gnss_cb_t      cb;
    void*              user_data;
} app_gnss_consumer_t;

static const char* LOG_TAG = "asuna_gnss";

static List_t s_app_gnss_consumer_list;

static void app_gnss_uart_event_task(void* parameters);
static void app_gnss_reset(void);
static void app_gnss_dispatch(app_gnss_cb_type_t type, void* data);

int app_gnss_server_init(void) {
    app_gnss_server_ctx_t* ctx = malloc(sizeof(app_gnss_server_ctx_t));
    if (ctx == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to allocate GNSS context.");

        return -1;
    }

    memset(ctx, 0x00U, sizeof(app_gnss_server_ctx_t));

    gpio_config_t pin_conf = {
        .pin_bit_mask = (1U << GNSS_RST_PIN),
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    gpio_config(&pin_conf);

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

    vListInitialise(&s_app_gnss_consumer_list);

    app_gnss_reset();

    if (xTaskCreate(app_gnss_uart_event_task, "A_GNSS", 4096, ctx, 5, &ctx->uart_rx_task) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create GNSS UART event task.");
        return -1;
    }

    ESP_LOGI(LOG_TAG, "GNSS server initialization completed.");
    return 0;
}

app_gnss_cb_handle_t app_gnss_server_cb_register(app_gnss_cb_type_t type, app_gnss_cb_t callback, void* handle) {
    app_gnss_consumer_t* consumer = malloc(sizeof(app_gnss_consumer_t));
    if (consumer == NULL) {
        return NULL;
    }

    consumer->type      = type;
    consumer->cb        = callback;
    consumer->user_data = handle;

    ListItem_t* item = malloc(sizeof(ListItem_t));
    if (item == NULL) {
        goto free_consumer_exit;
    }

    vListInitialiseItem(item);

    item->xItemValue = (TickType_t)consumer;

    vListInsert(&s_app_gnss_consumer_list, item);

    return consumer;

free_consumer_exit:
    free(consumer);

    return NULL;
}

void app_gnss_server_cb_unregister(app_gnss_cb_handle_t handle) {
    ListItem_t*       item = listGET_HEAD_ENTRY(&s_app_gnss_consumer_list);
    const ListItem_t* end  = listGET_END_MARKER(&s_app_gnss_consumer_list);

    while (item != NULL) {
        if (item == end) {
            break;
        }

        app_gnss_consumer_t* consumer = (app_gnss_consumer_t*)item->xItemValue;

        if (consumer == handle) {
            listREMOVE_ITEM(item);
            free(consumer);
            free(item);

            break;
        }

        item = listGET_NEXT(item);
    }
}

static void app_gnss_reset(void) {
    gpio_set_level(GNSS_RST_PIN, 0U);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GNSS_RST_PIN, 1U);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void app_gnss_uart_event_task(void* parameters) {
    int                    i;
    app_gnss_server_ctx_t* ctx = parameters;
    uart_event_t           event;

    for (;;) {
        if (xQueueReceive(ctx->uart_rx_queue, &event, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (event.type != UART_DATA) {
            switch (event.type) {
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

            continue;
        }

        /* Only UART data event exists from now on. */

        ESP_LOGD(LOG_TAG, "UART data event.");

        size_t   gnss_data_size;
        uint8_t* gnss_data_buf;
        uart_get_buffered_data_len(GNSS_UART_NUM, &gnss_data_size);

        gnss_data_buf = malloc(gnss_data_size);
        if (gnss_data_buf == NULL) {
            ESP_LOGE(LOG_TAG, "Failed to allocate GNSS content buffer.");
            continue;
        }

        uart_read_bytes(GNSS_UART_NUM, gnss_data_buf, gnss_data_size, portMAX_DELAY);

        for (i = 0; i < gnss_data_size; i++) {
            if (input_nmea(&ctx->nmea_raw, gnss_data_buf[i])) {
                ESP_LOGD(LOG_TAG, "NMEA[%c%c%c] received", ctx->nmea_raw.type[0], ctx->nmea_raw.type[1],
                         ctx->nmea_raw.type[2]);
            }

            if (nl_input_rtcm3_v2(&ctx->rtcm_raw, gnss_data_buf[i])) {
                ESP_LOGD(LOG_TAG, "RTCM[%d] received", ctx->rtcm_raw.type);
            }
        }

        free(gnss_data_buf);

        /* Dispatch */
        app_gnss_dispatch(APP_GNSS_CB_FIX, NULL);
        app_gnss_dispatch(APP_GNSS_CB_SAT, NULL);
        app_gnss_dispatch(APP_GNSS_CB_RAW_NMEA, NULL);
        app_gnss_dispatch(APP_GNSS_CB_RAW_RTCM, NULL);
    }
}

static void app_gnss_dispatch(app_gnss_cb_type_t type, void* data) {
    ListItem_t*       item = listGET_HEAD_ENTRY(&s_app_gnss_consumer_list);
    const ListItem_t* end  = listGET_END_MARKER(&s_app_gnss_consumer_list);

    while (item != NULL) {
        if (item == end) {
            break;
        }

        app_gnss_consumer_t* consumer = (app_gnss_consumer_t*)item->xItemValue;
        consumer->cb(consumer->user_data, type, data);

        item = listGET_NEXT(item);
    }
}
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
#define GNSS_PPS_PIN       CONFIG_APP_GNSS_SERVER_PPS_GPIO

typedef struct {
    QueueHandle_t uart_rx_queue;
    TaskHandle_t  uart_rx_task;
    TaskHandle_t  pps_event_task;

    nmea_raw_t nmea_raw;
    nl_rtcm_t  rtcm_raw;

    List_t consumer_list;
} app_gnss_server_state_t;

typedef struct {
    app_gnss_cb_type_t type;
    app_gnss_cb_t      cb;
    void*              user_data;
} app_gnss_consumer_t;

static const char* LOG_TAG = "asuna_gnss";

static const char* s_app_gnss_init_commands[] = {
    "$PAIR862,0,0,253*2E\r\n",
    "$PAIR092,1*2C\r\n",
    "$PAIR513*3D\r\n",
};

static app_gnss_server_state_t s_app_gnss_server_state;

static void app_gnss_pps_event_task(void* parameters);
static void app_gnss_uart_event_task(void* parameters);
static void app_gnss_reset(void);
static void app_gnss_pps_isr_handler(void* arg);
static void app_gnss_send_init_commands(void);
static void app_gnss_dispatch(app_gnss_cb_type_t type, void* data);

int app_gnss_server_init(void) {
    gpio_config_t pin_conf = {
        .pin_bit_mask = (1U << GNSS_RST_PIN),
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    gpio_config(&pin_conf);

    pin_conf.pin_bit_mask = (1U << GNSS_PPS_PIN);
    pin_conf.intr_type    = GPIO_INTR_POSEDGE;
    pin_conf.mode         = GPIO_MODE_INPUT;

    gpio_config(&pin_conf);

    gpio_isr_handler_add(GNSS_PPS_PIN, app_gnss_pps_isr_handler, &s_app_gnss_server_state);

    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_set_pin(GNSS_UART_NUM, GNSS_TX_PIN, GNSS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GNSS_UART_NUM, GNSS_UART_BUF_SIZE, GNSS_UART_BUF_SIZE, 8,
                        &s_app_gnss_server_state.uart_rx_queue, 0);
    uart_param_config(GNSS_UART_NUM, &uart_config);

    vListInitialise(&s_app_gnss_server_state.consumer_list);

    if (xTaskCreate(app_gnss_uart_event_task, "asuna_gnss", 4096, &s_app_gnss_server_state, 5,
                    &s_app_gnss_server_state.uart_rx_task) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create GNSS UART event task.");
        return -1;
    }

    if (xTaskCreate(app_gnss_pps_event_task, "asuna_pps", 4096, &s_app_gnss_server_state, 6,
                    &s_app_gnss_server_state.pps_event_task) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create GNSS PPS event task.");
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

    vListInsert(&s_app_gnss_server_state.consumer_list, item);

    return consumer;

free_consumer_exit:
    free(consumer);

    return NULL;
}

void app_gnss_server_cb_unregister(app_gnss_cb_handle_t handle) {
    ListItem_t*       item = listGET_HEAD_ENTRY(&s_app_gnss_server_state.consumer_list);
    const ListItem_t* end  = listGET_END_MARKER(&s_app_gnss_server_state.consumer_list);

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

static void app_gnss_send_init_commands(void) {
    const size_t num_commands = sizeof(s_app_gnss_init_commands) / sizeof(s_app_gnss_init_commands[0]);

    for (int i = 0; i < num_commands; i++) {
        uart_write_bytes(GNSS_UART_NUM, s_app_gnss_init_commands[i], strlen(s_app_gnss_init_commands[i]));
        ESP_LOGD(LOG_TAG, "Sent command: %s", s_app_gnss_init_commands[i]);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(LOG_TAG, "Sent all initialization commands to GNSS module");
}

static void app_gnss_reset(void) {
    gpio_set_level(GNSS_RST_PIN, 0U);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GNSS_RST_PIN, 1U);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

static void app_gnss_pps_isr_handler(void* arg) {
    const app_gnss_server_state_t* state = arg;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTaskNotifyFromISR(state->pps_event_task, BIT0, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void app_gnss_uart_event_task(void* parameters) {
    esp_err_t                ret;
    app_gnss_server_state_t* state = parameters;
    uart_event_t             event;

    app_gnss_reset();
    app_gnss_send_init_commands();

    for (;;) {
        if (xQueueReceive(state->uart_rx_queue, &event, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (event.type != UART_DATA) {
            switch (event.type) {
                case UART_FIFO_OVF: {
                    ESP_LOGE(LOG_TAG, "Hardware FIFO overflowed...");
                    uart_flush_input(GNSS_UART_NUM);

                    xQueueReset(state->uart_rx_queue);
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
        ret = uart_get_buffered_data_len(GNSS_UART_NUM, &gnss_data_size);

        if (ret != ESP_OK || gnss_data_size == 0) {
            ESP_LOGD(LOG_TAG, "Received UART data event without data, continue...");
            continue;
        }

        gnss_data_buf = malloc(gnss_data_size);
        if (gnss_data_buf == NULL) {
            ESP_LOGE(LOG_TAG, "Failed to allocate GNSS content buffer.");
            continue;
        }

        gnss_data_size = uart_read_bytes(GNSS_UART_NUM, gnss_data_buf, gnss_data_size, portMAX_DELAY);
        if (gnss_data_size <= 0) {
            ESP_LOGE(LOG_TAG, "Not enough data read from UART.");
            goto free_buf_continue;
        }

        for (size_t i = 0; i < gnss_data_size; i++) {
            if (input_nmea(&state->nmea_raw, gnss_data_buf[i])) {
                ESP_LOGD(LOG_TAG, "NMEA[%c%c%c] received", state->nmea_raw.type[0], state->nmea_raw.type[1],
                         state->nmea_raw.type[2]);
            }

            if (nl_input_rtcm3_v2(&state->rtcm_raw, gnss_data_buf[i])) {
                ESP_LOGD(LOG_TAG, "RTCM[%d] received", state->rtcm_raw.type);

                app_gnss_rtcm_t rtcm = {
                    .type     = state->rtcm_raw.type,
                    .data     = state->rtcm_raw.buf,
                    .data_len = state->rtcm_raw.len,
                };

                app_gnss_dispatch(APP_GNSS_CB_RAW_RTCM, &rtcm);
            }
        }

        /* Dispatch */
        app_gnss_dispatch(APP_GNSS_CB_FIX, NULL);
        app_gnss_dispatch(APP_GNSS_CB_SAT, NULL);
        app_gnss_dispatch(APP_GNSS_CB_RAW_NMEA, NULL);

    free_buf_continue:
        free(gnss_data_buf);
    }
}

static void app_gnss_pps_event_task(void* parameters) {
    uint32_t notify_value;
    for (;;) {
        if (xTaskNotifyWait(0UL, 0xFFFFFFFFUL, &notify_value, portMAX_DELAY) != pdPASS) {
            continue;
        }

        ESP_LOGD(LOG_TAG, "GNSS PPS event.");

        app_gnss_pps_t pps;

        pps.gps_year   = (uint16_t)s_app_gnss_server_state.nmea_raw.rmc.year;
        pps.gps_month  = (uint16_t)s_app_gnss_server_state.nmea_raw.rmc.mouth;
        pps.gps_day    = (uint16_t)s_app_gnss_server_state.nmea_raw.rmc.day;
        pps.gps_hour   = (uint16_t)s_app_gnss_server_state.nmea_raw.rmc.hour;
        pps.gps_minute = (uint16_t)s_app_gnss_server_state.nmea_raw.rmc.min;
        pps.gps_second = (uint16_t)s_app_gnss_server_state.nmea_raw.rmc.sec;

        app_gnss_dispatch(APP_GNSS_CB_PPS, &pps);
    }
}

static void app_gnss_dispatch(app_gnss_cb_type_t type, void* data) {
    /* TODO: Add a mutex here to protect resources. */

    ListItem_t*       item = listGET_HEAD_ENTRY(&s_app_gnss_server_state.consumer_list);
    const ListItem_t* end  = listGET_END_MARKER(&s_app_gnss_server_state.consumer_list);

    while (item != NULL) {
        if (item == end) {
            break;
        }

        app_gnss_consumer_t* consumer = (app_gnss_consumer_t*)item->xItemValue;

        if (consumer->type & type) {
            consumer->cb(consumer->user_data, type, data);
        }

        item = listGET_NEXT(item);
    }
}
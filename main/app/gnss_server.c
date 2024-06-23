#include <string.h>

/* IDF */
#include "esp_event.h"
#include "esp_log.h"

#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"

/* App */
#include "app/gnss_server.h"

/* nl */
#include "rcv/nmea.h"
#include "rcv/nl_rtcm3.h"

static const char *TAG = "gnss_server";

// ESP_EVENT_DEFINE_BASE(ESP_NMEA_EVENT);

#define GNSS_UART_BUF_SIZE (2048)

#define GNSS_UART_NUM      UART_NUM_2
#define TX_PIN GPIO_NUM_1
#define RX_PIN GPIO_NUM_2


typedef struct 
{
    QueueHandle_t uart_queue;
    TaskHandle_t  task_handle;
    nmea_raw_t nmea_raw;
    nl_rtcm_t rtcm_raw;
} app_gnss_server_ctx_t;

// static esp_event_loop_handle_t event_loop_hdl;        /*!< Event loop handle */


// static void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
// {
//     ESP_LOGI(TAG, "ID: 0x%X", (unsigned int)event_id);
// }


static void uart_event_task(void *pvParameters)
{
    int i;
    app_gnss_server_ctx_t* ctx = pvParameters;
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(GNSS_UART_BUF_SIZE);

    for (;;) 
    {
        if (xQueueReceive(ctx->uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            switch (event.type) 
            {
            case UART_DATA:
                uart_read_bytes(GNSS_UART_NUM, dtmp, event.size, portMAX_DELAY);

                for(i=0; i<event.size; i++)
                {
                    if(input_nmea(&ctx->nmea_raw, dtmp[i]))
                    {
                        ESP_LOGI(TAG, "NMEA[%c%c%c] received", ctx->nmea_raw.type[0], ctx->nmea_raw.type[1], ctx->nmea_raw.type[2]);
                    }

                    if(nl_input_rtcm3_v2(&ctx->rtcm_raw, dtmp[i]))
                    {
                         ESP_LOGI(TAG, "RTCM[%d] received", ctx->rtcm_raw.type);
                    }
                }
//                esp_event_post_to(event_loop_hdl, ESP_NMEA_EVENT, 1, NULL, 999, 0);
                break;
            case UART_FIFO_OVF:
                ESP_LOGE(TAG, "hw fifo overflow");
                uart_flush_input(GNSS_UART_NUM);
                xQueueReset(ctx->uart_queue);
                break;
            default:
                ESP_LOGE(TAG, "uart event type: %d", event.type);
                break;
            }
        }
 //       esp_event_loop_run(event_loop_hdl, pdMS_TO_TICKS(50));
    }
}


int app_gnss_server_init(void) 
{
    app_gnss_server_ctx_t* ctx = malloc(sizeof(app_gnss_server_ctx_t));

    memset(ctx, 0x00U, sizeof(app_gnss_server_ctx_t));

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
     uart_set_pin(GNSS_UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
     uart_driver_install(GNSS_UART_NUM, GNSS_UART_BUF_SIZE, GNSS_UART_BUF_SIZE, 8, &ctx->uart_queue, 0);
     uart_param_config(GNSS_UART_NUM, &uart_config);

    // esp_event_loop_args_t loop_args = 
    // {
    //     .queue_size = 8,
    //     .task_name = NULL
    // };

 //   esp_event_loop_create(&loop_args, &event_loop_hdl);
 //   esp_event_handler_register_with(event_loop_hdl, ESP_NMEA_EVENT, ESP_EVENT_ANY_ID, gps_event_handler, NULL);
                                           
    xTaskCreate(uart_event_task, "uart_event_task", 4096, ctx, 17, &ctx->task_handle);

    ESP_LOGI(TAG, "gnss_server initialization completed.");

    return 0;
}


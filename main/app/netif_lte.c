#include <string.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

/* FreeRTOS+ Config */
#include "cellular_config.h"
/* FreeRTOS+ */
#include "cellular_api.h"

/* App */
#include "app/netif_lte.h"

#define LTE_UART_NUM      UART_NUM_1
#define LTE_UART_BUF_SIZE (1024)
#define LTE_EN_PIN        CONFIG_APP_NETIF_LTE_EN_GPIO
#define LTE_TX_PIN        CONFIG_APP_NETIF_LTE_TX_GPIO
#define LTE_RX_PIN        CONFIG_APP_NETIF_LTE_RX_GPIO
#define LTE_DTR_PIN       CONFIG_APP_NETIF_LTE_DTR_GPIO

static const char* LOG_TAG = "asuna_lte";

typedef struct {
    QueueHandle_t uart_rx_queue;
    TaskHandle_t  uart_rx_task;

    CellularCommInterfaceReceiveCallback_t recv_callback;
    void*                                  user_data;
} app_netif_lte_ctx_t;

static CellularHandle_t        s_cellular_handle = NULL;
static CellularCommInterface_t s_cellular_comm_interface;

static void                         app_netif_lte_pin_init(void);
static void                         app_netif_lte_reset(void);
static void                         app_netif_lte_manager_task(void* arguments);
static CellularCommInterfaceError_t app_netif_lte_comm_open(CellularCommInterfaceReceiveCallback_t receiveCallback,
                                                            void*                                  pUserData,
                                                            CellularCommInterfaceHandle_t* pCommInterfaceHandle);
static CellularCommInterfaceError_t app_netif_lte_comm_send(CellularCommInterfaceHandle_t commInterfaceHandle,
                                                            const uint8_t* pData, uint32_t dataLength,
                                                            uint32_t timeoutMilliseconds, uint32_t* pDataSentLength);
static CellularCommInterfaceError_t app_netif_lte_comm_recv(CellularCommInterfaceHandle_t commInterfaceHandle,
                                                            uint8_t* pBuffer, uint32_t bufferLength,
                                                            uint32_t  timeoutMilliseconds,
                                                            uint32_t* pDataReceivedLength);
static CellularCommInterfaceError_t app_netif_lte_comm_close(CellularCommInterfaceHandle_t commInterfaceHandle);

int app_netif_lte_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing...");

    app_netif_lte_pin_init();
    app_netif_lte_reset();

    s_cellular_comm_interface.open  = app_netif_lte_comm_open;
    s_cellular_comm_interface.close = app_netif_lte_comm_close;
    s_cellular_comm_interface.send  = app_netif_lte_comm_send;
    s_cellular_comm_interface.recv  = app_netif_lte_comm_recv;

    if (Cellular_Init(&s_cellular_handle, &s_cellular_comm_interface) != CELLULAR_SUCCESS) {
        ESP_LOGE(LOG_TAG, "Failed to initialize celluar network.");

        return -1;
    }

    if (xTaskCreate(app_netif_lte_manager_task, "A_LTE", 4096, s_cellular_handle, 3, NULL) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create LTE manager task.");

        return -2;
    }

    ESP_LOGI(LOG_TAG, "Initialization completed.");

    return 0;
}

static void app_netif_lte_pin_init(void) {
    gpio_config_t pin_conf = {
        .pin_bit_mask = (1U << LTE_EN_PIN),
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    gpio_config(&pin_conf);

    pin_conf.pin_bit_mask = 1U << LTE_DTR_PIN;
    pin_conf.mode         = GPIO_MODE_OUTPUT_OD;

    gpio_config(&pin_conf);

    gpio_set_level(LTE_DTR_PIN, 1U);
}

static void app_netif_lte_reset(void) {
    gpio_set_level(LTE_EN_PIN, 0U);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LTE_EN_PIN, 1U);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void app_netif_lte_manager_task(void* arguments) {
    CellularHandle_t handle = arguments;

    for (;;) {
        CellularSignalInfo_t signal_info;

        CellularError_t err = Cellular_GetSignalInfo(handle, &signal_info);
        if (err != CELLULAR_SUCCESS) {
            ESP_LOGE(LOG_TAG, "Failed to get signal info: %d", err);
        }

        ESP_LOGI(LOG_TAG, "Signal info: rssi: %d, ber: %d", signal_info.rssi, signal_info.ber);

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

static void app_netif_lte_comm_task(void* arguments) {
    app_netif_lte_ctx_t* ctx = arguments;

    uart_event_t event;
    for (;;) {
        if (xQueueReceive(ctx->uart_rx_queue, &event, portMAX_DELAY) == pdPASS) {
            switch (event.type) {
                case UART_DATA:
                    ESP_LOGD(LOG_TAG, "Received UART_DATA event.");
                    ctx->recv_callback(ctx->user_data, (CellularCommInterfaceHandle_t)ctx);
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGD(LOG_TAG, "Received UART_FIFO_OVF event.");
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGD(LOG_TAG, "Received UART_FRAME_ERR event.");
                    break;
                default:
                    ESP_LOGI(LOG_TAG, "Received unknown event: %d.", event.type);
                    break;
            }
        }
    }
}

static CellularCommInterfaceError_t app_netif_lte_comm_open(CellularCommInterfaceReceiveCallback_t receiveCallback,
                                                            void*                                  pUserData,
                                                            CellularCommInterfaceHandle_t* pCommInterfaceHandle) {
    ESP_LOGD(LOG_TAG, "LTE comm open");

    app_netif_lte_ctx_t* ctx = malloc(sizeof(app_netif_lte_ctx_t));
    if (ctx == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to allocate LTE comm context");

        return IOT_COMM_INTERFACE_FAILURE;
    }

    memset(ctx, 0x00U, sizeof(app_netif_lte_ctx_t));

    const uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ctx->recv_callback = receiveCallback;
    ctx->user_data     = pUserData;

    uart_driver_install(LTE_UART_NUM, LTE_UART_BUF_SIZE, LTE_UART_BUF_SIZE, 8, &ctx->uart_rx_queue, 0);
    uart_param_config(LTE_UART_NUM, &uart_config);
    uart_set_pin(LTE_UART_NUM, LTE_TX_PIN, LTE_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    if (xTaskCreate(app_netif_lte_comm_task, "A_COMM", 2048, ctx, 3, &ctx->uart_rx_task) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create LTE comm RX task");

        free(ctx);
        return IOT_COMM_INTERFACE_FAILURE;
    }

    *pCommInterfaceHandle = (CellularCommInterfaceHandle_t)ctx;

    return IOT_COMM_INTERFACE_SUCCESS;
}

static CellularCommInterfaceError_t app_netif_lte_comm_send(CellularCommInterfaceHandle_t commInterfaceHandle,
                                                            const uint8_t* pData, uint32_t dataLength,
                                                            uint32_t timeoutMilliseconds, uint32_t* pDataSentLength) {
    ESP_LOGD(LOG_TAG, "LTE command send: %ld bytes.", dataLength);

    *pDataSentLength = 0;

    *pDataSentLength = uart_write_bytes(LTE_UART_NUM, pData, dataLength);
    if (*pDataSentLength != dataLength) {
        ESP_LOGW(LOG_TAG, "LTE command transmit failed.");
        return IOT_COMM_INTERFACE_FAILURE;
    }

    if (uart_wait_tx_done(LTE_UART_NUM, pdMS_TO_TICKS(timeoutMilliseconds)) != ESP_OK) {
        ESP_LOGW(LOG_TAG, "LTE command wait for TX done timed out.");
        return IOT_COMM_INTERFACE_TIMEOUT;
    }

    return IOT_COMM_INTERFACE_SUCCESS;
}

static CellularCommInterfaceError_t app_netif_lte_comm_recv(CellularCommInterfaceHandle_t commInterfaceHandle,
                                                            uint8_t* pBuffer, uint32_t bufferLength,
                                                            uint32_t  timeoutMilliseconds,
                                                            uint32_t* pDataReceivedLength) {
    size_t rx_len;
    if (uart_get_buffered_data_len(LTE_UART_NUM, &rx_len) != ESP_OK) {
        goto err_read;
    }

    if (rx_len == 0) {
        *pDataReceivedLength = 0U;
        return IOT_COMM_INTERFACE_SUCCESS;
    }

    if (rx_len > bufferLength) {
        rx_len = bufferLength;
    }

    ESP_LOGD(LOG_TAG, "LTE command recv: %d bytes", rx_len);

    int ret = uart_read_bytes(LTE_UART_NUM, pBuffer, rx_len, pdMS_TO_TICKS(timeoutMilliseconds));
    if (ret < 0) {
        ESP_LOGE(LOG_TAG, "LTE command recv error: %d", ret);
        goto err_read;
    }

    *pDataReceivedLength = ret;
    return IOT_COMM_INTERFACE_SUCCESS;

err_read:
    *pDataReceivedLength = 0U;
    return IOT_COMM_INTERFACE_FAILURE;
}

static CellularCommInterfaceError_t app_netif_lte_comm_close(CellularCommInterfaceHandle_t commInterfaceHandle) {
    app_netif_lte_ctx_t* ctx = (app_netif_lte_ctx_t*)commInterfaceHandle;

    vTaskDelete(ctx->uart_rx_task);
    uart_driver_delete(LTE_UART_NUM);

    free(ctx);

    return IOT_COMM_INTERFACE_SUCCESS;
}

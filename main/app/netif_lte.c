/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"

/* App */
#include "app/netif_lte.h"

#define LTE_EN_PIN  CONFIG_APP_NETIF_LTE_EN_GPIO
#define LTE_TX_PIN  CONFIG_APP_NETIF_LTE_TX_GPIO
#define LTE_RX_PIN  CONFIG_APP_NETIF_LTE_RX_GPIO
#define LTE_DTR_PIN CONFIG_APP_NETIF_LTE_DTR_GPIO
#define LTE_APN     CONFIG_APP_NETIF_LTE_APN

static const char* LOG_TAG = "A_LTE";

static QueueHandle_t s_lte_ppp_event_queue = NULL;

static void app_netif_lte_pin_init(void);
static void app_netif_lte_reset(void);
static void app_netif_lte_ppp_event_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void app_netif_lte_task(void* arguments);

int app_netif_lte_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing...");

    app_netif_lte_pin_init();
    app_netif_lte_reset();

    s_lte_ppp_event_queue = xQueueCreate(4, sizeof(uint32_t));
    if (s_lte_ppp_event_queue == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create event queue.");

        return -1;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, app_netif_lte_ppp_event_cb, NULL));

    esp_modem_dce_config_t dce_config       = ESP_MODEM_DCE_DEFAULT_CONFIG(LTE_APN);
    esp_netif_config_t     netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t*           lte_netif        = esp_netif_new(&netif_ppp_config);

    if (lte_netif == NULL) {
        return -1;
    }

    esp_modem_dte_config_t dte_config       = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num        = LTE_TX_PIN;
    dte_config.uart_config.rx_io_num        = LTE_RX_PIN;
    dte_config.uart_config.rts_io_num       = -1;
    dte_config.uart_config.cts_io_num       = -1;
    dte_config.uart_config.flow_control     = ESP_MODEM_FLOW_CONTROL_NONE;
    dte_config.uart_config.rx_buffer_size   = 1024;
    dte_config.uart_config.tx_buffer_size   = 1024;
    dte_config.uart_config.event_queue_size = 4;
    dte_config.task_stack_size              = 1024;
    dte_config.task_priority                = 2;
    dte_config.dte_buffer_size              = 512;

    esp_modem_dce_t* dce = esp_modem_new(&dte_config, &dce_config, lte_netif);
    if (dce == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create modem.");
        return -2;
    }

    if (xTaskCreate(app_netif_lte_task, "LTE", 2048, dce, 3, NULL) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create LTE task.");
        return -3;
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

/* TODO: Find a way to detect PPP status and reconnect */

static void app_netif_lte_ppp_event_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ESP_LOGI(LOG_TAG, "PPP event ID received: %ld", event_id);
    if (xQueueSend(s_lte_ppp_event_queue, &event_id, 0) != pdPASS) {
        ESP_LOGW(LOG_TAG, "PPP event lost...");
    }
}

static void app_netif_lte_task(void* arguments) {
    esp_modem_dce_t* dce = arguments;

    char resp_buf[32];
    while (esp_modem_get_module_name(dce, resp_buf) != ESP_OK) {
        ESP_LOGW(LOG_TAG, "Get module name failed, perhaps not booted yet.");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(LOG_TAG, "Module name: %s", resp_buf);

    if (esp_modem_at(dce, "AT+CGMR", resp_buf, 500) != ESP_OK) {
        ESP_LOGW(LOG_TAG, "Failed to get module version.");
    } else {
        ESP_LOGI(LOG_TAG, "Module version: %s", resp_buf);
    }

    /* TODO: Handle No-SIM status. */

    esp_err_t ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Set module to data mode failed: %d", ret);
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#include <stdio.h>

#include "sdkconfig.h"

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

/* IDF */
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "spi_flash_mmap.h"

/* App */
#include "app/api_server.h"
#include "app/netif_common.h"
#include "app/netif_lte.h"
#include "app/netif_wifi.h"

static const char *LOG_TAG = "a_main";

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(LOG_TAG, "Project Asuna -- Initializing...");

    if (app_netif_init() != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize network interfaces.");

        goto dead_loop;
    }

    if (app_api_server_init() != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize web server.");

        goto dead_loop;
    }

    if (app_netif_wifi_init() != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize WiFi interface.");

        goto dead_loop;
    }

    if (app_netif_lte_init() != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize LTE interface.");

        goto dead_loop;
    }

    ESP_LOGI(LOG_TAG, "Initialization completed.");

dead_loop:
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

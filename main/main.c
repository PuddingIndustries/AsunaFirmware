#include <stdio.h>

#include "sdkconfig.h"

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

/* IDF */
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "spi_flash_mmap.h"

/* App */
#include "app/api_server.h"
#include "app/console_common.h"
#include "app/gnss_server.h"
#include "app/netif_common.h"
#include "app/netif_lte.h"
#include "app/netif_wifi.h"
#include "app/version_manager.h"
#include "app/vfs_common.h"

#define APP_ERROR_CHECK(x, m)                                 \
    do {                                                      \
        if (x != 0) {                                         \
            ESP_LOGE(LOG_TAG, "Failed to initialize %s.", m); \
            goto dead_loop;                                   \
        }                                                     \
    } while (0)

static const char *LOG_TAG = "asuna_main";

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

    ESP_LOGI(LOG_TAG, "Project Asuna -- Initializing...");

    APP_ERROR_CHECK(app_vfs_common_init(), "virtual file system");
    APP_ERROR_CHECK(app_version_manager_init(), "version manager");
    APP_ERROR_CHECK(app_console_init(), "console");
    APP_ERROR_CHECK(app_netif_init(), "network interfaces");
    APP_ERROR_CHECK(app_netif_wifi_init(), "WiFi interface");
    APP_ERROR_CHECK(app_netif_lte_init(), "LTE interface");
    APP_ERROR_CHECK(app_gnss_server_init(), "GNSS server");
    APP_ERROR_CHECK(app_api_server_init(), "web server");

    ESP_LOGI(LOG_TAG, "Initialization completed.");

    vTaskDelete(NULL);

dead_loop:
    for (;;) {
        vTaskSuspend(NULL);
    }
}

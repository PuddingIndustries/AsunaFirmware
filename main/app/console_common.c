#include <stdio.h>
#include <string.h>

/* IDF */
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "linenoise/linenoise.h"
#include "nvs.h"
#include "nvs_flash.h"

/* App */
#include "app/console/cmd_free.h"
#include "app/console/cmd_ip.h"
#include "app/console/cmd_ps.h"
#include "app/console/cmd_reset.h"
#include "app/console/cmd_version.h"
#include "app/console/cmd_wifi.h"
#include "app/console/cmd_gnss.h"
#include "app/console_common.h"

#define APP_CONSOLE_PROMPT_STR "asuna"

static const char* LOG_TAG = "asuna_console";

static const esp_console_cmd_t* s_app_console_cmd_list[] = {
    &app_console_cmd_free,
    &app_console_cmd_gnss,
    &app_console_cmd_ip,
    &app_console_cmd_ps,
    &app_console_cmd_reset,
    &app_console_cmd_version,
    &app_console_cmd_wifi,
};

int app_console_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing...");

    esp_console_repl_t*       repl        = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = APP_CONSOLE_PROMPT_STR ">";

    esp_console_register_help_command();

    for (size_t i = 0; i < (sizeof(s_app_console_cmd_list) / sizeof(s_app_console_cmd_list[0])); i++) {
        if (esp_console_cmd_register(s_app_console_cmd_list[i])) {
            ESP_LOGE(LOG_TAG, "Failed to register command %s.", s_app_console_cmd_list[i]->command);
        }
    }

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#else
#error "Either ESP_CONSOLE_USB_SERIAL_JTAG or ESP_CONSOLE_USB_CDC shall be selected."
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    return 0;
}

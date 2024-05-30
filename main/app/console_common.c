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
#include "app/console/cmd_wifi.h"
#include "app/console_common.h"

#define APP_CONSOLE_PROMPT_STR "asuna"

static const char* LOG_TAG = "asuna_console";

int app_console_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing...");

    esp_console_repl_t*       repl        = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = APP_CONSOLE_PROMPT_STR ">";

    esp_console_register_help_command();

    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    return 0;
}

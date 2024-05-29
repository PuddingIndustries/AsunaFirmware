#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "cmd_nvs.h"

static const char* LOG_TAG = "app_console";
#define PROMPT_STR "asuna"

int app_console_init(void)
{
    ESP_LOGI(LOG_TAG, "Initializing...");

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";

    /* Register commands */
    esp_console_register_help_command();
    register_system_common();
    register_system_sleep();
#if SOC_WIFI_SUPPORTED
    register_wifi();
#endif
    register_nvs();

    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

    linenoiseSetDumbMode(1);
    linenoiseSetMultiLine(0);

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    return 0;
}


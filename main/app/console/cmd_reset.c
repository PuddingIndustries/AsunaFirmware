/* IDF */
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "linenoise/linenoise.h"
#include "nvs.h"
#include "nvs_flash.h"

/* App */
#include "app/console/cmd_reset.h"

static int app_console_reset_func(int argc, char **argv) {
    printf("Erasing NVS configuration and restart...\n");

    nvs_flash_erase();
    esp_restart();
    return 0;
}

const esp_console_cmd_t app_console_cmd_reset = {
    .command = "reset",
    .help    = "Reset module to factory defaults",
    .hint    = NULL,
    .func    = app_console_reset_func,
};
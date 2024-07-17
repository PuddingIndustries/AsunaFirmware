/* IDF */
#include "esp_app_desc.h"
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
#include "app/console/cmd_version.h"

static int app_console_version_func(int argc, char **argv) {
    const esp_app_desc_t *app_desc = esp_app_get_description();

    printf("IDF Version:%s\n", esp_get_idf_version());
    printf("App Version: %s\n", app_desc->version);
    return 0;
}

const esp_console_cmd_t app_console_cmd_version = {
    .command = "version",
    .help    = "Get version of chip and SDK",
    .hint    = NULL,
    .func    = app_console_version_func,
};

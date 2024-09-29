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
#include "app/version_manager.h"

static inline void app_console_version_print(app_version_t *version) {
    printf("\t\tProject name: %s\n", version->name);
    printf("\t\tAPP version: %s\n", version->app_version);
    printf("\t\tIDF version: %s\n", version->idf_version);
    printf("\t\tCompiled at: %s %s\n", version->date, version->time);
    printf("\t\tSHA256: %s\n", (char *)version->sha256);
}

static int app_console_version_func(int argc, char **argv) {
    printf("Version information:\n");

    for (size_t i = 0; i < APP_OTA_SLOT_END; i++) {
        app_version_t version;

        if (app_version_manager_get_status(i, &version) != 0) {
            return -1;
        }

        printf("\tOTA slot #%d [%c]:\n", i, version.is_current ? '+' : '-');
        if (version.is_valid) {
            app_console_version_print(&version);
        } else {
            printf("\t\tSlot is empty or invalid.\n");
        }
        printf("\n");
    }

    return 0;
}

const esp_console_cmd_t app_console_cmd_version = {
    .command = "version",
    .help    = "Get version of chip and SDK",
    .hint    = NULL,
    .func    = app_console_version_func,
};

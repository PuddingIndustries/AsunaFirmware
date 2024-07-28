#include <string.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_log.h"
#include "esp_ota_ops.h"

/* App */
#include "app/version_manager.h"

int app_version_manager_init(void) {
    return 0;
}

int app_version_manager_get_status(app_ota_slot_t slot, app_version_t *version) {
    const esp_app_desc_t  *app_desc = esp_app_get_description();
    const esp_partition_t *app_part = esp_ota_get_running_partition();

    memset(version, 0, sizeof(app_version_t));

    switch (app_part->subtype) {
        case ESP_PARTITION_SUBTYPE_APP_OTA_0: {
            if (slot == APP_OTA_SLOT_0) version->is_current = true;
            break;
        }

        case ESP_PARTITION_SUBTYPE_APP_OTA_1: {
            if (slot == APP_OTA_SLOT_1) version->is_current = true;
            break;
        }

        default:
            break;
    }

    strncpy(version->name, app_desc->project_name, sizeof(version->name));
    strncpy(version->app_version, app_desc->version, sizeof(version->app_version));
    strncpy(version->idf_version, app_desc->idf_ver, sizeof(version->idf_version));
    strncpy(version->date, app_desc->date, sizeof(version->date));
    strncpy(version->time, app_desc->time, sizeof(version->time));

    for (size_t i = 0; i < sizeof(app_desc->app_elf_sha256); i++) {
        snprintf(&version->sha256[2 * i], 3, "%02x", app_desc->app_elf_sha256[i]);
    }

    return 0;
}

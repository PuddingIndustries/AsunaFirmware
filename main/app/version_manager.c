#include <string.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* IDF */
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

/* App */
#include "app/version_manager.h"

#define APP_VM_OTA_TIMEOUT_MS (30 * 1000)

typedef struct {
    SemaphoreHandle_t      upgrade_mutex;
    esp_ota_handle_t       upgrade_handle;
    size_t                 upgrade_received;
    const esp_partition_t *upgrade_partition;
} app_version_manager_state_t;

static const char *LOG_TAG = "asuna_vermgr";

static app_version_manager_state_t s_app_vm_state;

int app_version_manager_init(void) {
    s_app_vm_state.upgrade_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_app_vm_state.upgrade_mutex == NULL) {
        return -1;
    }

    s_app_vm_state.upgrade_handle   = 0;
    s_app_vm_state.upgrade_received = 0;

    return 0;
}

int app_version_manager_get_status(app_ota_slot_t slot, app_version_t *version) {
    const esp_app_desc_t  *app_desc = esp_app_get_description();
    const esp_partition_t *app_part = esp_ota_get_running_partition();
    const esp_partition_t *nxt_part = esp_ota_get_next_update_partition(app_part);

    /* TODO: Find the next partition and check for its version. */

    memset(version, 0, sizeof(app_version_t));

    switch (app_part->subtype) {
        case ESP_PARTITION_SUBTYPE_APP_OTA_0: {
            if (slot == APP_OTA_SLOT_0) version->state = APP_OTA_SLOT_STATE_IN_USE;
            break;
        }

        case ESP_PARTITION_SUBTYPE_APP_OTA_1: {
            if (slot == APP_OTA_SLOT_1) version->state = APP_OTA_SLOT_STATE_IN_USE;
            break;
        }

        default:
            break;
    }

    if (version->state == APP_OTA_SLOT_STATE_IN_USE) {
        strncpy(version->name, app_desc->project_name, sizeof(version->name));
        strncpy(version->app_version, app_desc->version, sizeof(version->app_version));
        strncpy(version->idf_version, app_desc->idf_ver, sizeof(version->idf_version));
        strncpy(version->date, app_desc->date, sizeof(version->date));
        strncpy(version->time, app_desc->time, sizeof(version->time));

        for (size_t i = 0; i < sizeof(app_desc->app_elf_sha256); i++) {
            snprintf(&version->sha256[2 * i], 3, "%02x", app_desc->app_elf_sha256[i]);
        }
    } else {
        esp_app_desc_t nxt_desc;
        if (esp_ota_get_partition_description(nxt_part, &nxt_desc) != ESP_OK) {
            version->state = APP_OTA_SLOT_STATE_EMPTY;

            /* Do not copy invalid values here. */
        } else {
            uint8_t sha256_buf[32];

            /* Validate image. */

            if (esp_partition_get_sha256(nxt_part, sha256_buf) == ESP_ERR_IMAGE_INVALID) {
                version->state = APP_OTA_SLOT_STATE_INVALID;
            } else {
                version->state = APP_OTA_SLOT_STATE_READY;
            }

            strncpy(version->name, nxt_desc.project_name, sizeof(version->name));
            strncpy(version->app_version, nxt_desc.version, sizeof(version->app_version));
            strncpy(version->idf_version, nxt_desc.idf_ver, sizeof(version->idf_version));
            strncpy(version->date, nxt_desc.date, sizeof(version->date));
            strncpy(version->time, nxt_desc.time, sizeof(version->time));

            for (size_t i = 0; i < sizeof(sha256_buf); i++) {
                snprintf(&version->sha256[2 * i], 3, "%02x", sha256_buf[i]);
            }
        }
    }

    return 0;
}

int app_version_manager_ota_start(void) {
    if (xSemaphoreTakeRecursive(s_app_vm_state.upgrade_mutex, pdMS_TO_TICKS(APP_VM_OTA_TIMEOUT_MS)) != pdPASS) {
        ESP_LOGW(LOG_TAG, "OTA session is acquired by another session.");
        return -1;
    }

    const esp_partition_t *app_part = esp_ota_get_running_partition();
    const esp_partition_t *nxt_part = esp_ota_get_next_update_partition(app_part);

    s_app_vm_state.upgrade_partition = nxt_part;

    ESP_LOGI(LOG_TAG, "Next OTA partition: %s", nxt_part->label);

    s_app_vm_state.upgrade_handle   = 0;
    s_app_vm_state.upgrade_received = 0;

    esp_err_t ret = esp_ota_begin(nxt_part, 0, &s_app_vm_state.upgrade_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to initialize OTA session.");
        return -1;
    }

    ESP_LOGI(LOG_TAG, "OTA session started.");

    return 0;
}

int app_version_manager_ota_save(const uint8_t *data, size_t length) {
    if (xSemaphoreTakeRecursive(s_app_vm_state.upgrade_mutex, pdMS_TO_TICKS(APP_VM_OTA_TIMEOUT_MS)) != pdPASS) {
        ESP_LOGW(LOG_TAG, "OTA session is acquired by another session.");
        return -1;
    }

    esp_err_t ret = esp_ota_write(s_app_vm_state.upgrade_handle, data, length);
    if (ret != ESP_OK) {
        ESP_LOGW(LOG_TAG, "Failed to write OTA data.");

        goto release_lock_exit;
    }

    s_app_vm_state.upgrade_received += length;

    xSemaphoreGiveRecursive(s_app_vm_state.upgrade_mutex);
    return 0;

release_lock_exit:
    xSemaphoreGiveRecursive(s_app_vm_state.upgrade_mutex);
    return -1;
}

int app_version_manager_ota_commit(void) {
    if (esp_ota_end(s_app_vm_state.upgrade_handle) != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to end OTA session.");
    }

    esp_ota_set_boot_partition(s_app_vm_state.upgrade_partition);

    esp_restart();

    return 0;
}

int app_version_manager_ota_abort(void) {
    if (s_app_vm_state.upgrade_handle) {
        esp_ota_abort(s_app_vm_state.upgrade_handle);
    }

    xSemaphoreGiveRecursive(s_app_vm_state.upgrade_mutex);

    return 0;
}

/* IDF */
#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_system.h"

/* App */
#include "app/vfs_common.h"

#define APP_VFS_MP    "/storage"
#define APP_VFS_LABEL "storage"

static const char *LOG_TAG = "asuna_vfs";

int app_vfs_common_init(void) {
    esp_err_t err;

    esp_vfs_littlefs_conf_t conf = {
        .base_path              = APP_VFS_MP,
        .partition_label        = APP_VFS_LABEL,
        .format_if_mount_failed = true,
        .dont_mount             = false,
    };

    err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to register LFS.");
        return -1;
    }

    size_t fs_total;
    size_t fs_used;

    err = esp_littlefs_info(conf.partition_label, &fs_total, &fs_used);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to get LFS info.");
        goto unregister_exit;
    }

    ESP_LOGI(LOG_TAG, "LFS stat: %d/%d bytes used.", fs_used, fs_total);

    return 0;

unregister_exit:
    esp_vfs_littlefs_unregister(conf.partition_label);
    return -1;
}
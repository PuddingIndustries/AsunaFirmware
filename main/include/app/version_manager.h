#ifndef APP_VERSION_MANAGER_H
#define APP_VERSION_MANAGER_H

typedef enum {
    APP_OTA_SLOT_0,
    APP_OTA_SLOT_1,
    APP_OTA_SLOT_END,
} app_ota_slot_t;

typedef enum {
    APP_OTA_SLOT_STATE_EMPTY,
    APP_OTA_SLOT_STATE_READY,
    APP_OTA_SLOT_STATE_IN_USE,
    APP_OTA_SLOT_STATE_INVALID,
} app_ota_slot_state_t;

typedef struct {
    app_ota_slot_state_t state;

    char name[32];
    char date[16];
    char time[16];
    char app_version[32];
    char idf_version[32];
    char sha256[64 + 12];
} app_version_t;

int app_version_manager_init(void);
int app_version_manager_get_status(app_ota_slot_t slot, app_version_t *version);
int app_version_manager_ota_start(void);
int app_version_manager_ota_save(const uint8_t *data, size_t length);
int app_version_manager_ota_commit(void);
int app_version_manager_ota_abort(void);

#endif  // APP_VERSION_MANAGER_H

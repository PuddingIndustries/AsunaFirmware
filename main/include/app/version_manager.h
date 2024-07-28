#ifndef APP_VERSION_MANAGER_H
#define APP_VERSION_MANAGER_H

typedef enum {
    APP_OTA_SLOT_0,
    APP_OTA_SLOT_1,
} app_ota_slot_t;

typedef struct {
    bool is_current;

    char name[32];
    char date[16];
    char time[16];
    char app_version[32];
    char idf_version[32];
    char sha256[64 + 12];
} app_version_t;

int app_version_manager_init(void);
int app_version_manager_get_status(app_ota_slot_t slot, app_version_t *version);

#endif  // APP_VERSION_MANAGER_H

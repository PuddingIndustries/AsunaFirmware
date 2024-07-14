#ifndef APP_GNSS_SERVER_H
#define APP_GNSS_SERVER_H

typedef enum {
    APP_GNSS_CB_FIX,
    APP_GNSS_CB_SAT,
    APP_GNSS_CB_RAW_NMEA,
    APP_GNSS_CB_RAW_RTCM,
} app_gnss_cb_type_t;

typedef struct {
    double latitude;
    double longitude;
    double altitude;
} app_gnss_fix_t;

typedef void *app_gnss_cb_handle_t;
typedef int (*app_gnss_cb_t)(void *handle, app_gnss_cb_type_t type, void *payload);

int                  app_gnss_server_init(void);
app_gnss_cb_handle_t app_gnss_server_cb_register(app_gnss_cb_type_t type, app_gnss_cb_t cb, void *handle);
void                 app_gnss_server_cb_unregister(app_gnss_cb_handle_t handle);

#endif  // APP_GNSS_SERVER_H
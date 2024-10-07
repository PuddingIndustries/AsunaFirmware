#ifndef APP_GNSS_SERVER_H
#define APP_GNSS_SERVER_H

typedef enum {
    APP_GNSS_CB_FIX      = 1 << 0U,
    APP_GNSS_CB_SAT      = 1 << 1U,
    APP_GNSS_CB_RAW_NMEA = 1 << 2U,
    APP_GNSS_CB_RAW_RTCM = 1 << 3U,
    APP_GNSS_CB_PPS      = 1 << 4U,
} app_gnss_cb_type_t;

typedef struct {
    double latitude;
    double longitude;
    double altitude;

    /* TODO: Add more fields. */
} app_gnss_fix_t;

typedef struct {
    uint16_t type;
    size_t   data_len;
    uint8_t *data;
} app_gnss_rtcm_t;

typedef struct {
    char     type[3];
    size_t   data_len;
    uint8_t *data;
} app_gnss_nmea_t;

typedef struct {
    uint16_t gps_year;
    uint16_t gps_month;
    uint16_t gps_day;
    uint16_t gps_hour;
    uint16_t gps_minute;
    uint16_t gps_second;
} app_gnss_pps_t;

typedef void *app_gnss_cb_handle_t;
typedef int   (*app_gnss_cb_t)(void *handle, app_gnss_cb_type_t type, void *payload);

int                  app_gnss_server_init(void);
app_gnss_cb_handle_t app_gnss_server_cb_register(app_gnss_cb_type_t type, app_gnss_cb_t cb, void *handle);
void                 app_gnss_server_cb_unregister(app_gnss_cb_handle_t handle);

#endif  // APP_GNSS_SERVER_H
#include <string.h>
#include "esp_console.h"
#include "app/gnss_server.h"

/* App */
#include "app/console/cmd_gnss_server_test.h"

static app_gnss_cb_handle_t g_gnss_handle = NULL;

static int gnss_callback(void* user_data, app_gnss_cb_type_t type, void* data) {
    switch (type) {
        case APP_GNSS_CB_FIX:
            printf("GNSS Fix received\n");

            break;
        case APP_GNSS_CB_SAT:
            printf("GNSS Satellite info received\n");

            break;
        case APP_GNSS_CB_RAW_NMEA:
            printf("Raw NMEA data received\n");

            break;
        case APP_GNSS_CB_RAW_RTCM:
            printf("Raw RTCM data received\n");

            break;
        default:
            printf("Unknown GNSS data type received\n");
    }

    return 0;
}

static int cmd_gnss_test(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: gnss_test [start|stop]\n");
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        if (g_gnss_handle == NULL) {
            printf("Starting GNSS data monitoring...\n");
            g_gnss_handle = app_gnss_server_cb_register(APP_GNSS_CB_FIX | APP_GNSS_CB_SAT | APP_GNSS_CB_RAW_NMEA | APP_GNSS_CB_RAW_RTCM, gnss_callback, NULL);
            if (g_gnss_handle == NULL) {
                printf("Failed to register GNSS callback\n");
                return 1;
            }
            printf("GNSS data monitoring started. Waiting for data...\n");
        } else {
            printf("GNSS data monitoring is already active\n");
        }
    } else if (strcmp(argv[1], "stop") == 0) {
        if (g_gnss_handle != NULL) {
            printf("Stopping GNSS data monitoring...\n");
            app_gnss_server_cb_unregister(g_gnss_handle);
            g_gnss_handle = NULL;
            printf("GNSS data monitoring stopped\n");
        } else {
            printf("GNSS data monitoring is not active\n");
        }
    } else {
        printf("Invalid argument. Use 'start' or 'stop'\n");
        return 1;
    }

    return 0;
}

const esp_console_cmd_t app_console_cmd_gnss_test = {
    .command = "gnss_test",
    .help = "Test GNSS server functionality. Usage: gnss_test [start|stop]",
    .hint = NULL,
    .func = &cmd_gnss_test,
};



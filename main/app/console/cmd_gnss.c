#include <errno.h>
#include <string.h>

#include "app/gnss_server.h"
#include "esp_console.h"

/* App */
#include "app/console/cmd_gnss.h"
#include "app/console/private.h"

static int app_console_gnss_subcommand_help(int argc, char **argv);
static int app_console_gnss_subcommand_test(int argc, char **argv);

static const app_console_subcommand_t s_app_console_gnss_subcommands[] = {
    {.command = "help", .handler = app_console_gnss_subcommand_help},
    {.command = "test", .handler = app_console_gnss_subcommand_test},
};

static int app_console_gnss_event_callback(void *user_data, app_gnss_cb_type_t type, void *data) {
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
        case APP_GNSS_CB_RAW_RTCM: {
            const app_gnss_rtcm_t *rtcm = data;
            printf("Raw RTCM data received: type: 0x%04x, len: %u\n", rtcm->type, rtcm->data_len);

            break;
        }

        case APP_GNSS_CB_PPS: {
            const app_gnss_pps_t *pps = data;
            printf("GNSS PPS event received: %04d-%02u-%02u %02u:%02u:%02u\n", pps->gps_year, pps->gps_month,
                   pps->gps_day, pps->gps_hour, pps->gps_minute, pps->gps_second);
            break;
        }

        default:
            printf("Unknown GNSS data type received\n");
    }

    return 0;
}

static int app_console_gnss_subcommand_help(int argc, char **argv) {
    printf("Usage: gnss <command> [options...]\n");
    printf("Commands:\n");
    printf("\thelp: Print this help.\n");
    printf("\ttest: Start GNSS data capture and dump to terminal.\n");

    if (argv != NULL) {
        return 0;
    }

    return -1;
}

static int app_console_gnss_subcommand_test(int argc, char **argv) {
    /* TODO: Filter callback types by arguments */

    if (argc != 2) {
        printf("Usage: gnss test <FILTER_BITMAP>\n");
        printf("Filters:\n");
        printf("\tBIT0: NMEA fix\n");
        printf("\tBIT1: NMEA satellite list\n");
        printf("\tBIT2: NMEA raw data\n");
        printf("\tBIT3: RTCM raw data\n");
        printf("\tBIT4: PPS\n");

        return -1;
    }

    long cb_type = strtol(argv[1], NULL, 0);
    if (errno != 0) {
        int err = errno;
        printf("Invalid filter bitmap: %s (%s)\n", argv[1], strerror(err));

        return -2;
    }

    app_gnss_cb_handle_t handle = NULL;

    printf("Start GNSS data monitoring, press any key to stop...\n");
    handle = app_gnss_server_cb_register(cb_type, app_console_gnss_event_callback, NULL);

    getchar();

    app_gnss_server_cb_unregister(handle);

    printf("GNSS data monitoring stopped.\n");

    return 0;
}

static int app_console_gnss_func(int argc, char **argv) {
    if (argc <= 1) {
        return app_console_gnss_subcommand_help(0, NULL);
    }

    char  *cmd            = argv[1];
    size_t commands_count = sizeof(s_app_console_gnss_subcommands) / sizeof(s_app_console_gnss_subcommands[0]);

    for (size_t i = 0; i < commands_count; i++) {
        if (strcmp(cmd, s_app_console_gnss_subcommands[i].command) != 0) {
            continue;
        }

        return s_app_console_gnss_subcommands[i].handler(argc - 1, &argv[1]);
    }

    return 0;
}

const esp_console_cmd_t app_console_cmd_gnss = {
    .command = "gnss",
    .help    = "GNSS control and debug command",
    .hint    = NULL,
    .func    = &app_console_gnss_func,
};

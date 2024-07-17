#include <string.h>

/* IDF */
#include "esp_console.h"

/* App */
#include "app/console/cmd_wifi.h"
#include "app/netif_wifi.h"

typedef int (*app_console_wifi_subcommand_t)(int argc, char **argv);

typedef struct {
    const char                   *command;
    app_console_wifi_subcommand_t handler;
} app_console_wifi_handler_t;

static int app_console_wifi_subcommand_help(int argc, char **argv);
static int app_console_wifi_subcommand_status(int argc, char **argv);

static const app_console_wifi_handler_t s_app_console_wifi_handlers[] = {
    {.command = "help", .handler = app_console_wifi_subcommand_help},
    {.command = "status", .handler = app_console_wifi_subcommand_status},
};

static int app_console_wifi_func(int argc, char **argv) {
    if (argc <= 1) {
        return app_console_wifi_subcommand_help(0, NULL);
    }

    char  *cmd            = argv[1];
    size_t commands_count = sizeof(s_app_console_wifi_handlers) / sizeof(s_app_console_wifi_handlers[0]);

    for (size_t i = 0; i < commands_count; i++) {
        if (strcmp(cmd, s_app_console_wifi_handlers[i].command) != 0) {
            continue;
        }

        return s_app_console_wifi_handlers[i].handler(argc - 1, &argv[1]);
    }

    return 0;
}

static int app_console_wifi_subcommand_help(int argc, char **argv) {
    printf("Usage: wifi <command> [options...]\n");
    printf("Commands:\n");
    printf("\thelp: Print this help.\n");
    printf("\tstatus: Show Wi-Fi status.\n");

    if (argv != NULL) {
        return 0;
    }

    return -1;
}

static int app_console_wifi_subcommand_status(int argc, char **argv) {
    app_netif_wifi_config_t config;
    app_netif_wifi_status_t status;

    int ret;

    ret = app_netif_wifi_config_get(&config);
    if (ret != 0) {
        printf("Failed to retrieve Wi-Fi configuration.\n");

        return -1;
    }

    ret = app_netif_wifi_status_get(&status);
    if (ret != 0) {
        printf("Failed to retrieve Wi-Fi status.\n");

        return -1;
    }

    printf("Wi-Fi status: \n");

    printf("\tStation status: ");
    if (config.sta_enabled) {
        if (status.sta_status.connected) {
            printf("connected.\n");

            printf("\t\tSSID: %s\n", status.sta_status.ssid);
            printf("\t\tRSSI: %ddBm\n", status.sta_status.rssi);
        } else {
            printf("connecting...\n");
        }
    } else {
        printf("disabled.\n");
    }

    printf("\n");
    printf("\tAP status: ");
    if (config.ap_enabled) {
        printf("enabled.\n");

        printf("\t\tSSID: %s\n", config.ap_config.ssid);
        printf("\t\tConnected clients: %d\n", status.ap_status.client_count);
    } else {
        printf("disabled.\n");
    }

    return 0;
}

const esp_console_cmd_t app_console_cmd_wifi = {
    .command = "wifi",
    .help    = "Wi-Fi control command",
    .hint    = NULL,
    .func    = app_console_wifi_func,
};
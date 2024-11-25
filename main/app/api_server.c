#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* App */
#include "app/api/config/handler_lora.h"
#include "app/api/config/handler_upgrade.h"
#include "app/api/config/handler_wifi.h"
#include "app/api/gnss/handler_stream.h"
#include "app/api/handler_static.h"
#include "app/api/status/handler_network.h"
#include "app/api/status/handler_wifi.h"
#include "app/api_server.h"

typedef struct {
    char              *name;
    const httpd_uri_t *uri;
    int (*init)(void);
    int (*onopen)(httpd_handle_t handle, int fd);
    int (*onclose)(httpd_handle_t handle, int fd);
} app_api_server_handler_t;

static const char *LOG_TAG = "asuna_httpsrv";

static const app_api_server_handler_t s_app_handler_list[] = {
    {
        .name    = "config_lora_get",
        .uri     = &app_api_config_handler_lora_get_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "config_lora_post",
        .uri     = &app_api_config_handler_lora_post_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "config_upgrade_get",
        .uri     = &app_api_config_handler_upgrade_get_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "config_upgrade_post",
        .uri     = &app_api_config_handler_upgrade_post_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "config_wifi_get",
        .uri     = &app_api_config_handler_wifi_get_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "config_wifi_post",
        .uri     = &app_api_config_handler_wifi_post_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "status_network_get",
        .uri     = &app_api_status_handler_network_get_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "status_wifi_get",
        .uri     = &app_api_status_handler_wifi_get_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    },
    {
        .name    = "gnss_stream_ws",
        .uri     = &app_api_gnss_handler_stream_ws_uri,
        .init    = app_api_gnss_handler_stream_ws_init,
        .onopen  = app_api_gnss_handler_stream_ws_onopen,
        .onclose = app_api_gnss_handler_stream_ws_onclose,
    },
    {
        .name    = "default_get",
        .uri     = &app_api_handler_static_default_get_uri,
        .init    = NULL,
        .onopen  = NULL,
        .onclose = NULL,
    }, /* Wildcard match, must be the last one */
};

static httpd_handle_t s_app_api_handle = NULL;

static esp_err_t app_api_server_socket_open_callback(httpd_handle_t handle, int fd);
static void      app_api_server_socket_close_callback(httpd_handle_t handle, int fd);

int app_api_server_init(void) {
    int ret = 0;

    ESP_LOGI(LOG_TAG, "Starting httpd...");

    const size_t handler_count = sizeof(s_app_handler_list) / sizeof(s_app_handler_list[0]);

    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();

    httpd_config.task_priority    = 2;
    httpd_config.max_uri_handlers = handler_count;
    httpd_config.uri_match_fn     = httpd_uri_match_wildcard;
    httpd_config.open_fn          = app_api_server_socket_open_callback;
    httpd_config.close_fn         = app_api_server_socket_close_callback;

    s_app_api_handle = NULL;

    if (httpd_start(&s_app_api_handle, &httpd_config) != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to start http server.");
        return -1;
    }

    for (size_t i = 0; i < handler_count; i++) {
        const app_api_server_handler_t *handler = &s_app_handler_list[i];
        if (handler->init != NULL) {
            if (s_app_handler_list[i].init() != 0) {
                ESP_LOGE(LOG_TAG, "Failed to initialize URI handler %s.", handler->name);

                ret = -1;

                goto deinit_server_exit;
            }
        }

        if (httpd_register_uri_handler(s_app_api_handle, handler->uri) != ESP_OK) {
            ESP_LOGE(LOG_TAG, "Failed to register URI handler %s.", handler->name);

            ret = -2;

            goto deinit_server_exit;
        }
    }

    ESP_LOGI(LOG_TAG, "Web server initialized.");

    return 0;

deinit_server_exit:
    httpd_stop(s_app_api_handle);

    return ret;
}

static esp_err_t app_api_server_socket_open_callback(httpd_handle_t handle, int fd) {
    ESP_LOGD(LOG_TAG, "Underlying socket open, fd=%d", fd);

    size_t handler_count = sizeof(s_app_handler_list) / sizeof(s_app_handler_list[0]);

    for (size_t i = 0; i < handler_count; i++) {
        const app_api_server_handler_t *handler = &s_app_handler_list[i];
        if (handler->onopen != NULL) {
            esp_err_t ret = s_app_handler_list[i].onopen(handle, fd);
            if (ret != 0) {
                ESP_LOGE(LOG_TAG, "Failed to execute URI onOpen() handler %s.", handler->name);

                return ret;
            }
        }
    }

    return ESP_OK;
}

static void app_api_server_socket_close_callback(httpd_handle_t handle, int fd) {
    ESP_LOGD(LOG_TAG, "Underlying socket closed, fd=%d", fd);

    size_t handler_count = sizeof(s_app_handler_list) / sizeof(s_app_handler_list[0]);

    for (size_t i = 0; i < handler_count; i++) {
        const app_api_server_handler_t *handler = &s_app_handler_list[i];
        if (handler->onclose != NULL) {
            s_app_handler_list[i].onclose(handle, fd);
        }
    }

    /* HTTPD does not close socket if close_fn is set; DO NOT REMOVE! */
    close(fd);
}

int app_api_server_deinit(void) {
    if (s_app_api_handle) {
        httpd_stop(s_app_api_handle);
    }

    return 0;
}
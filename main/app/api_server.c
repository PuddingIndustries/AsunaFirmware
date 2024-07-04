#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* App */
#include "app/api/config/handler_wifi.h"
#include "app/api/gnss/handler_stream.h"
#include "app/api/handler_index.h"
#include "app/api/handler_static.h"
#include "app/api_server.h"

typedef struct {
    char              *name;
    const httpd_uri_t *uri;
    int                (*init)(void);
} app_api_server_handler_t;

static const char *LOG_TAG = "asuna_httpsrv";

static const app_api_server_handler_t s_app_uri_list[] = {
    {.name = "index_get", .uri = &app_api_handler_index_get_uri, .init = NULL},
    {.name = "static_get", .uri = &app_api_handler_static_get_uri, .init = NULL},
    {.name = "config_wifi_get", .uri = &app_api_config_handler_wifi_get_uri, .init = NULL},
    {.name = "config_wifi_post", .uri = &app_api_config_handler_wifi_post_uri, .init = NULL},
    {.name = "gnss_stream_ws", .uri = &app_api_gnss_handler_stream_ws_uri, .init = NULL},
};

static httpd_handle_t s_app_api_handle = NULL;

int app_api_server_init(void) {
    int ret = 0;

    ESP_LOGI(LOG_TAG, "Starting httpd...");

    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();

    httpd_config.uri_match_fn = httpd_uri_match_wildcard;

    s_app_api_handle = NULL;

    if (httpd_start(&s_app_api_handle, &httpd_config) != ESP_OK) {
        return -1;
    }

    size_t handler_count = sizeof(s_app_uri_list) / sizeof(s_app_uri_list[0]);

    for (size_t i = 0; i < handler_count; i++) {
        const app_api_server_handler_t *handler = &s_app_uri_list[i];
        if (handler->init != NULL) {
            if (s_app_uri_list[i].init() != 0) {
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

int app_api_server_deinit(void) {
    if (s_app_api_handle) {
        httpd_stop(s_app_api_handle);
    }

    return 0;
}
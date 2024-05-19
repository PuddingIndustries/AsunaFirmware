#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* App */
#include "app/api/handler_index.h"
#include "app/api/handler_static.h"
#include "app/api_server.h"

typedef struct {
    char              *name;
    int                (*init)(void);
    const httpd_uri_t *uri;
} app_api_server_handler_t;

static const char *LOG_TAG = "A_HTTP";

static const app_api_server_handler_t s_app_uri_list[] = {
    {.name = "INDEX", .init = app_api_handler_index_init, .uri = &app_api_handler_index_uri},
    {.name = "STATIC", .init = app_api_handler_static_init, .uri = &app_api_handler_static_uri},
};

static httpd_handle_t s_app_api_handle = NULL;

int app_api_server_init(void) {
    int ret = 0;

    ESP_LOGI(LOG_TAG, "Starting httpd...");

    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    s_app_api_handle            = NULL;

    if (httpd_start(&s_app_api_handle, &httpd_config) != ESP_OK) {
        return -1;
    }

    size_t handler_count = sizeof(s_app_uri_list) / sizeof(s_app_uri_list[0]);

    for (size_t i = 0; i < handler_count; i++) {
        if (s_app_uri_list[i].init() != 0) {
            ESP_LOGE(LOG_TAG, "Failed to initialize URI handler %s.", s_app_uri_list[i].name);

            ret = -1;

            goto deinit_server_exit;
        }

        if (httpd_register_uri_handler(s_app_api_handle, s_app_uri_list[i].uri) != ESP_OK) {
            ESP_LOGE(LOG_TAG, "Failed to register URI handler %s.", s_app_uri_list[i].name);

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
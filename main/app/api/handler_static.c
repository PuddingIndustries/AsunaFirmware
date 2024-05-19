#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* App */
#include "app/api/handler_static.h"

int app_api_handler_static_init(void) {
    return 0;
}

static esp_err_t app_api_handler_static_get(httpd_req_t *req) {
    return ESP_OK;
}

const httpd_uri_t app_api_handler_static_uri = {
    .uri      = "/static/*",
    .method   = HTTP_GET,
    .handler  = app_api_handler_static_get,
    .user_ctx = NULL,
};

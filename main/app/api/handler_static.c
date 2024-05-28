#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* App */
#include "app/api/handler_static.h"

typedef struct {
    char *file_ext;
    char *mime_type;
} app_mime_type_t;

static const app_mime_type_t s_app_mime_types[] = {
    {.file_ext = "html", .mime_type = "text/html"},
    {.file_ext = "css", .mime_type = "text/css"},
    {.file_ext = "js", .mime_type = "text/javascript"},
};

static esp_err_t app_api_handler_static_get(httpd_req_t *req) {
    (void)s_app_mime_types;

    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, "Try later...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t app_api_handler_static_get_uri = {
    .uri      = "/static/*",
    .method   = HTTP_GET,
    .handler  = app_api_handler_static_get,
    .user_ctx = NULL,
};

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* Frontend */
#include "frontend_manager.h"

/* App */
#include "app/api/handler_static.h"

typedef enum {
    APP_API_HANDLER_RSRC_INDEX,
    APP_API_HANDLER_RSRC_ASSETS,
} app_api_handler_static_resource_type_t;

static esp_err_t app_api_handler_static_get(httpd_req_t *req) {
    int file_id = -1;

    for (size_t i = 0; i < frontend_mgr_get_file_count(); i++) {
        const char *f_path = frontend_mgr_get_file_path(i);
        if (strncasecmp(f_path, req->uri, strlen(f_path)) == 0) {
            file_id = (int)i;
            break;
        }
    }

    if (file_id < 0) {
        goto return_404;
    }

    const size_t   file_size = frontend_mgr_get_file_size(file_id);
    const uint8_t *file_data = frontend_mgr_get_file_data(file_id);
    const char    *file_mime = frontend_mgr_get_file_mime_type(file_id);

    httpd_resp_set_type(req, file_mime);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    httpd_resp_send(req, (const char *)file_data, (ssize_t)file_size);

    return ESP_OK;
return_404:

    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, "Try later...", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
}

const httpd_uri_t app_api_handler_static_default_get_uri = {
    .uri      = "*",
    .method   = HTTP_GET,
    .handler  = app_api_handler_static_get,
    .user_ctx = (void *)APP_API_HANDLER_RSRC_INDEX,
};
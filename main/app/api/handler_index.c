#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* App */
#include "app/api/handler_index.h"

static esp_err_t app_api_handler_index_get(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Location", "/static/index.html");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_send(req, "Redirecting...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t app_api_handler_index_get_uri = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = app_api_handler_index_get,
    .user_ctx = NULL,
};

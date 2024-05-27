#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* App */
#include "app/api/config/handler_wifi.h"

int app_api_config_handler_wifi_init(void) {
    return 0;
}

static esp_err_t app_api_config_handler_wifi_get(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Location", "/static/index.html");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_send(req, "Redirecting...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t app_api_config_handler_wifi_post(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Location", "/static/index.html");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_send(req, "Redirecting...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


const httpd_uri_t app_api_config_handler_wifi_get_uri = {
    .uri      = "/api/config/wifi",
    .method   = HTTP_GET,
    .handler  = app_api_config_handler_wifi_get,
    .user_ctx = NULL,
};

const httpd_uri_t app_api_config_handler_wifi_post_uri = {
    .uri      = "/api/config/wifi",
    .method   = HTTP_POST,
    .handler  = app_api_config_handler_wifi_post,
    .user_ctx = NULL,
};

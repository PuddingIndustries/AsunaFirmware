#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* cJSON */
#include "cJSON.h"

/* App */
#include "app/api/config/handler_wifi.h"
#include "app/netif_wifi.h"

static char *app_api_config_handler_wifi_serialize(const app_netif_wifi_config_t *config) {
    char *ret = NULL;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON *root_ap = cJSON_CreateObject();
    if (root_ap == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "ap", root_ap);

    cJSON *root_ap_enabled = cJSON_CreateBool(config->ap_enabled);
    if (root_ap_enabled == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_ap, "enabled", root_ap_enabled);

    cJSON *root_ap_ssid = cJSON_CreateString(config->ap_config.ssid);
    if (root_ap_ssid == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_ap, "ssid", root_ap_ssid);

    cJSON *root_ap_pass = cJSON_CreateString(config->ap_config.pass);
    if (root_ap_pass == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_ap, "pass", root_ap_pass);

    cJSON *root_ap_chan = cJSON_CreateNumber(config->ap_config.chan);
    if (root_ap_chan == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_ap, "chan", root_ap_chan);

    cJSON *root_sta = cJSON_CreateObject();
    if (root_sta == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "sta", root_sta);

    cJSON *root_sta_enabled = cJSON_CreateBool(config->sta_enabled);
    if (root_sta_enabled == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_sta, "enabled", root_sta_enabled);

    cJSON *root_sta_ssid = cJSON_CreateString(config->sta_config.ssid);
    if (root_sta_ssid == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_sta, "ssid", root_sta_ssid);

    cJSON *root_sta_pass = cJSON_CreateString(config->sta_config.pass);
    if (root_sta_pass == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_sta, "pass", root_sta_pass);

    ret = cJSON_PrintUnformatted(root);

del_root_exit:
    cJSON_Delete(root);
    return ret;
}

static esp_err_t app_api_config_handler_wifi_get(httpd_req_t *req) {
    app_netif_wifi_config_t wifi_config;

    if (app_netif_wifi_config_get(&wifi_config) != 0) {
        goto send_500;
    }

    char *json = app_api_config_handler_wifi_serialize(&wifi_config);
    if (json == NULL) {
        goto send_500;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    cJSON_free(json);
    return ESP_OK;

send_500:
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"

/* cJSON */
#include "cJSON.h"

/* App */
#include "app/api/status/handler_wifi.h"
#include "app/netif_wifi.h"

static char *app_api_status_handler_wifi_serialize(const app_netif_wifi_status_t *status) {
    char *ret = NULL;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON *root_ap = cJSON_CreateObject();
    if (root_ap == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "ap", root_ap);

    cJSON *root_ap_clients = cJSON_CreateNumber(status->ap_status.client_count);
    if (root_ap_clients == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_ap, "clients", root_ap_clients);

    cJSON *root_sta = cJSON_CreateObject();
    if (root_sta == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "sta", root_sta);

    cJSON *root_sta_connected = cJSON_CreateBool(status->sta_status.connected);
    if (root_sta_connected == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_sta, "connected", root_sta_connected);

    if (status->sta_status.connected) {
        cJSON *root_sta_ssid = cJSON_CreateString(status->sta_status.ssid);
        if (root_sta_ssid == NULL) goto del_root_exit;
        cJSON_AddItemToObject(root_sta, "ssid", root_sta_ssid);

        cJSON *root_sta_bssid = cJSON_CreateString(status->sta_status.bssid);
        if (root_sta_bssid == NULL) goto del_root_exit;
        cJSON_AddItemToObject(root_sta, "bssid", root_sta_bssid);

        cJSON *root_sta_rssi = cJSON_CreateNumber(status->sta_status.rssi);
        if (root_sta_rssi == NULL) goto del_root_exit;
        cJSON_AddItemToObject(root_sta, "rssi", root_sta_rssi);
    }

    ret = cJSON_PrintUnformatted(root);

del_root_exit:
    cJSON_Delete(root);
    return ret;
}

static esp_err_t app_api_status_handler_wifi_get(httpd_req_t *req) {
    app_netif_wifi_status_t wifi_status;

    if (app_netif_wifi_status_get(&wifi_status) != 0) {
        goto send_500;
    }

    char *json = app_api_status_handler_wifi_serialize(&wifi_status);
    if (json == NULL) goto send_500;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    cJSON_free(json);

    return ESP_OK;

send_500:
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
}

const httpd_uri_t app_api_status_handler_wifi_get_uri = {
    .uri      = "/api/status/wifi",
    .method   = HTTP_GET,
    .handler  = app_api_status_handler_wifi_get,
    .user_ctx = NULL,
};
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

#define APP_HANDLER_WIFI_MAXIMUM_PAYLOAD_SIZE 256

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

static app_netif_wifi_config_t *app_api_config_handler_wifi_deserialize(const char *json) {
    char *buf = NULL;

    app_netif_wifi_config_t *cfg = malloc(sizeof(app_netif_wifi_config_t));
    if (cfg == NULL) {
        return NULL;
    }

    cJSON *j = cJSON_Parse(json);
    if (j == NULL) {
        goto free_obj_exit;
    }

    /* Now we have a complete tree, building AP configuration struct */

    cJSON *root_ap = cJSON_GetObjectItem(j, "ap");
    if (cJSON_IsInvalid(root_ap) || !cJSON_IsObject(root_ap)) {
        goto del_json_exit;
    }

    cJSON *root_ap_enabled = cJSON_GetObjectItem(root_ap, "enabled");
    if (cJSON_IsInvalid(root_ap_enabled)) goto del_json_exit;

    cfg->ap_enabled = cJSON_IsTrue(root_ap_enabled);

    cJSON *root_ap_ssid = cJSON_GetObjectItem(root_ap, "ssid");
    if (cJSON_IsInvalid(root_ap_ssid)) goto del_json_exit;

    buf = cJSON_GetStringValue(root_ap_ssid);
    if (buf == NULL) {
        goto del_json_exit;
    }

    strncpy(cfg->ap_config.ssid, buf, sizeof(cfg->ap_config.ssid));

    cJSON *root_ap_pass = cJSON_GetObjectItem(root_ap, "pass");
    if (cJSON_IsInvalid(root_ap_pass)) goto del_json_exit;

    buf = cJSON_GetStringValue(root_ap_pass);
    if (buf == NULL) goto del_json_exit;

    strncpy(cfg->ap_config.pass, buf, sizeof(cfg->ap_config.pass));

    cJSON *root_ap_chan = cJSON_GetObjectItem(root_ap, "chan");
    if (cJSON_IsInvalid(root_ap_chan)) goto del_json_exit;

    cfg->ap_config.chan = cJSON_GetNumberValue(root_ap_chan);

    /* Build STA configuration here... */

    cJSON *root_sta = cJSON_GetObjectItem(j, "sta");
    if (cJSON_IsInvalid(root_sta) || !cJSON_IsObject(root_sta)) goto del_json_exit;

    cJSON *root_sta_enabled = cJSON_GetObjectItem(root_sta, "enabled");
    if (cJSON_IsInvalid(root_sta_enabled)) goto del_json_exit;

    cfg->sta_enabled = cJSON_IsTrue(root_sta_enabled);

    cJSON *root_sta_ssid = cJSON_GetObjectItem(root_sta, "ssid");
    if (cJSON_IsInvalid(root_sta_ssid)) goto del_json_exit;

    buf = cJSON_GetStringValue(root_sta_ssid);
    if (buf == NULL) goto del_json_exit;

    strncpy(cfg->sta_config.ssid, buf, sizeof(cfg->sta_config.ssid));

    cJSON *root_sta_pass = cJSON_GetObjectItem(root_sta, "pass");
    if (cJSON_IsInvalid(root_sta_pass)) goto del_json_exit;

    buf = cJSON_GetStringValue(root_sta_pass);
    if (buf == NULL) goto del_json_exit;

    strncpy(cfg->sta_config.pass, buf, sizeof(cfg->sta_config.pass));

    cJSON_Delete(j);

    return cfg;

del_json_exit:
    cJSON_Delete(j);

free_obj_exit:
    free(cfg);

    return NULL;
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
    size_t payload_size = req->content_len;
    if (payload_size > APP_HANDLER_WIFI_MAXIMUM_PAYLOAD_SIZE) {
        payload_size = APP_HANDLER_WIFI_MAXIMUM_PAYLOAD_SIZE;
    }

    char *payload = malloc(payload_size);
    if (payload == NULL) goto send_500;

    int ret = httpd_req_recv(req, payload, payload_size);
    if (ret <= 0) goto send_500;

    app_netif_wifi_config_t *cfg = app_api_config_handler_wifi_deserialize(payload);
    if (cfg == NULL) goto send_500;

    ret = app_netif_wifi_config_set(cfg);
    free(cfg);

    if (ret != 0) goto send_500;

    ret = app_netif_wifi_config_reload();
    if (ret != 0) goto send_500;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;

send_500:
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
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

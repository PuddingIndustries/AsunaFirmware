#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* cJSON */
#include "cJSON.h"

/* App */
#include "app/api/config/handler_lora.h"
#include "app/lora_server.h"

#define APP_HANDLER_LORA_MAXIMUM_PAYLOAD_SIZE 256

static char *app_api_config_handler_lora_serialize(const app_lora_server_config_t *config) {
    char *ret = NULL;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON *root_forward_rtcm = cJSON_CreateBool(config->fw_rtcm);
    if (root_forward_rtcm == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "forward_rtcm", root_forward_rtcm);

    cJSON *root_modem_config = cJSON_CreateObject();
    if (root_modem_config == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "modem_config", root_modem_config);

    cJSON *root_modem_config_freq = cJSON_CreateNumber((double)config->modem_config.frequency);
    if (root_modem_config_freq == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_modem_config, "freq", root_modem_config_freq);

    cJSON *root_modem_config_power = cJSON_CreateNumber(config->modem_config.power);
    if (root_modem_config_power == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_modem_config, "power", root_modem_config_power);

    cJSON *root_modem_config_type = cJSON_CreateNumber(config->modem_config.network_type);
    if (root_modem_config_type == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_modem_config, "type", root_modem_config_type);

    cJSON *root_modem_config_bw = cJSON_CreateNumber(config->modem_config.bandwidth);
    if (root_modem_config_bw == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_modem_config, "bw", root_modem_config_bw);

    cJSON *root_modem_config_sf = cJSON_CreateNumber(config->modem_config.spreading_factor);
    if (root_modem_config_sf == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_modem_config, "sf", root_modem_config_sf);

    cJSON *root_modem_config_cr = cJSON_CreateNumber(config->modem_config.coding_rate);
    if (root_modem_config_cr == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_modem_config, "cr", root_modem_config_cr);

    cJSON *root_modem_config_ldr_opt = cJSON_CreateBool(config->modem_config.ldr_optimization);
    if (root_modem_config_ldr_opt == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root_modem_config, "ldr_opt", root_modem_config_ldr_opt);

    ret = cJSON_PrintUnformatted(root);

del_root_exit:
    cJSON_Delete(root);
    return ret;
}

static app_lora_server_config_t *app_api_config_handler_lora_deserialize(const char *json) {
    app_lora_server_config_t *cfg = malloc(sizeof(app_lora_server_config_t));
    if (cfg == NULL) return NULL;

    cJSON *j = cJSON_Parse(json);
    if (j == NULL) {
        goto free_obj_exit;
    }

    cJSON *root_forward_rtcm = cJSON_GetObjectItem(j, "forward_rtcm");
    if (cJSON_IsInvalid(root_forward_rtcm) || !cJSON_IsBool(root_forward_rtcm)) {
        goto del_json_exit;
    }

    cfg->fw_rtcm = cJSON_IsTrue(root_forward_rtcm);

    cJSON *root_modem_config = cJSON_GetObjectItem(j, "modem_config");
    if (cJSON_IsInvalid(root_modem_config) || !cJSON_IsObject(root_modem_config)) {
        goto del_json_exit;
    }

    cJSON *root_modem_config_freq = cJSON_GetObjectItem(root_modem_config, "freq");
    if (cJSON_IsInvalid(root_modem_config_freq) || !cJSON_IsNumber(root_modem_config_freq)) {
        goto del_json_exit;
    }

    cfg->modem_config.frequency = (uint32_t)cJSON_GetNumberValue(root_modem_config_freq);

    cJSON *root_modem_config_power = cJSON_GetObjectItem(root_modem_config, "power");
    if (cJSON_IsInvalid(root_modem_config_power) || !cJSON_IsNumber(root_modem_config_power)) {
        goto del_json_exit;
    }

    cfg->modem_config.power = (uint8_t)cJSON_GetNumberValue(root_modem_config_power);

    cJSON *root_modem_config_type = cJSON_GetObjectItem(root_modem_config, "type");
    if (cJSON_IsInvalid(root_modem_config_type) || !cJSON_IsNumber(root_modem_config_type)) {
        goto del_json_exit;
    }

    cfg->modem_config.network_type = (uint8_t)cJSON_GetNumberValue(root_modem_config_type);

    cJSON *root_modem_config_bw = cJSON_GetObjectItem(root_modem_config, "bw");
    if (cJSON_IsInvalid(root_modem_config_bw) || !cJSON_IsNumber(root_modem_config_bw)) {
        goto del_json_exit;
    }

    cfg->modem_config.bandwidth = (uint8_t)cJSON_GetNumberValue(root_modem_config_bw);

    cJSON *root_modem_config_sf = cJSON_GetObjectItem(root_modem_config, "sf");
    if (cJSON_IsInvalid(root_modem_config_sf) || !cJSON_IsNumber(root_modem_config_sf)) {
        goto del_json_exit;
    }

    cfg->modem_config.spreading_factor = (uint8_t)cJSON_GetNumberValue(root_modem_config_sf);

    cJSON *root_modem_config_cr = cJSON_GetObjectItem(root_modem_config, "cr");
    if (cJSON_IsInvalid(root_modem_config_cr) || !cJSON_IsNumber(root_modem_config_cr)) {
        goto del_json_exit;
    }

    cfg->modem_config.coding_rate = (uint8_t)cJSON_GetNumberValue(root_modem_config_cr);

    cJSON *root_modem_config_ldr_opt = cJSON_GetObjectItem(root_modem_config, "ldr_opt");
    if (cJSON_IsInvalid(root_modem_config_ldr_opt) || !cJSON_IsBool(root_modem_config_ldr_opt)) {
        goto del_json_exit;
    }

    cfg->modem_config.ldr_optimization = cJSON_IsTrue(root_modem_config_ldr_opt);

    cJSON_Delete(j);

    return cfg;

del_json_exit:
    cJSON_Delete(j);

free_obj_exit:
    free(cfg);

    return NULL;
}

static esp_err_t app_api_config_handler_lora_get(httpd_req_t *req) {
    app_lora_server_config_t lora_config;

    if (app_lora_server_config_get(&lora_config) != 0) {
        goto send_500;
    }

    char *json = app_api_config_handler_lora_serialize(&lora_config);
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

static esp_err_t app_api_config_handler_lora_post(httpd_req_t *req) {
    size_t payload_size = req->content_len;
    if (payload_size > APP_HANDLER_LORA_MAXIMUM_PAYLOAD_SIZE) {
        payload_size = APP_HANDLER_LORA_MAXIMUM_PAYLOAD_SIZE;
    }

    char *payload = malloc(payload_size);
    if (payload == NULL) goto send_500;

    int ret = httpd_req_recv(req, payload, payload_size);
    if (ret < 0) goto send_500;

    app_lora_server_config_t *cfg = app_api_config_handler_lora_deserialize(payload);
    if (cfg == NULL) goto free_buf_send_500;

    free(payload);

    ret = app_lora_server_config_set(cfg);
    free(cfg);

    if (ret != 0) goto send_500;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;

free_buf_send_500:
    free(payload);

send_500:
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
}

const httpd_uri_t app_api_config_handler_lora_get_uri = {
    .uri      = "/api/config/lora",
    .method   = HTTP_GET,
    .handler  = app_api_config_handler_lora_get,
    .user_ctx = NULL,
};

const httpd_uri_t app_api_config_handler_lora_post_uri = {
    .uri      = "/api/config/lora",
    .method   = HTTP_POST,
    .handler  = app_api_config_handler_lora_post,
    .user_ctx = NULL,
};
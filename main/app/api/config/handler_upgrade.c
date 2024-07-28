#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* cJSON */
#include "cJSON.h"

/* App */
#include "app/api/config/handler_upgrade.h"
#include "app/version_manager.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const app_ota_slot_t s_ota_slots[] = {APP_OTA_SLOT_0, APP_OTA_SLOT_1};

static char *app_api_config_handler_upgrade_serialize(const app_version_t *versions, size_t num_slots) {
    char *ret = NULL;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON *versions_array = cJSON_CreateArray();
    if (versions_array == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "versions", versions_array);

    for (size_t i = 0; i < num_slots; i++) {
        cJSON *version = cJSON_CreateObject();
        if (version == NULL) goto del_root_exit;
        cJSON_AddItemToArray(versions_array, version);

        cJSON *name = cJSON_CreateString(versions[i].name);
        if (name == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "name", name);

        cJSON *date = cJSON_CreateString(versions[i].date);
        if (date == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "date", date);

        cJSON *time = cJSON_CreateString(versions[i].time);
        if (time == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "time", time);

        cJSON *app_version = cJSON_CreateString(versions[i].app_version);
        if (app_version == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "app_version", app_version);

        cJSON *idf_version = cJSON_CreateString(versions[i].idf_version);
        if (idf_version == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "idf_version", idf_version);

        cJSON *sha256 = cJSON_CreateString(versions[i].sha256);
        if (sha256 == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "sha256", sha256);
    }

    ret = cJSON_PrintUnformatted(root);

del_root_exit:
    cJSON_Delete(root);
    return ret;
}

static esp_err_t app_api_config_handler_upgrade_get(httpd_req_t *req) {
    app_version_t versions[ARRAY_SIZE(s_ota_slots)];

    for (size_t i = 0; i < ARRAY_SIZE(s_ota_slots); i++) {
        if (app_version_manager_get_status(APP_OTA_SLOT_0, &versions[i]) != 0) {
            goto send_500;
        }
    }

    char *json = app_api_config_handler_upgrade_serialize(versions, ARRAY_SIZE(s_ota_slots));
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

static esp_err_t app_api_config_handler_upgrade_post(httpd_req_t *req) {
    /* TODO: Handle firmware upload. */

    return ESP_OK;
}

const httpd_uri_t app_api_config_handler_upgrade_get_uri = {
    .uri      = "/api/config/upgrade",
    .method   = HTTP_GET,
    .handler  = app_api_config_handler_upgrade_get,
    .user_ctx = NULL,
};

const httpd_uri_t app_api_config_handler_upgrade_post_uri = {
    .uri      = "/api/config/upgrade",
    .method   = HTTP_POST,
    .handler  = app_api_config_handler_upgrade_post,
    .user_ctx = NULL,
};

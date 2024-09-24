#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* cJSON */
#include "cJSON.h"

/* Multipart Parser */
#include "multipart_parser.h"

/* App */
#include "app/api/config/handler_upgrade.h"
#include "app/version_manager.h"

#define APP_CONTENT_TYPE_MAX_LENGTH   512
#define APP_CONTENT_TYPE_VALID_VALUE  ("Multipart/form-data")
#define APP_CONTENT_TYPE_VALID_LENGTH (sizeof(APP_CONTENT_TYPE_VALID_VALUE) - 1)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

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

        cJSON *slot = cJSON_CreateNumber(i);
        if (slot == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "slot", slot);

        cJSON *in_use = cJSON_CreateBool(versions[i].is_current);
        if (in_use == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "in_use", in_use);

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
    app_version_t versions[APP_OTA_SLOT_END];

    for (size_t i = 0; i < APP_OTA_SLOT_END; i++) {
        if (app_version_manager_get_status(i, &versions[i]) != 0) {
            goto send_500;
        }
    }

    char *json = app_api_config_handler_upgrade_serialize(versions, APP_OTA_SLOT_END);
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

static int upgrade_post_on_field_callback(multipart_parser *parser, const char *at, size_t length) {
    return 0;
}

static int upgrade_post_on_data_callback(multipart_parser *parser, const char *at, size_t length) {
    return 0;
}

static esp_err_t app_api_config_handler_upgrade_post(httpd_req_t *req) {
    /* TODO: Handle firmware upload. */
    size_t content_type_length = httpd_req_get_hdr_value_len(req, "Content-Type");
    if (content_type_length == 0 || content_type_length > APP_CONTENT_TYPE_MAX_LENGTH) {
        goto send_500;
    }

    char *content_type = malloc(content_type_length + 1);
    if (content_type == NULL) goto send_500;

    if (strncmp(APP_CONTENT_TYPE_VALID_VALUE, content_type, APP_CONTENT_TYPE_VALID_LENGTH) != 0) {
        goto free_header_send_400;
    }

    multipart_parser_settings parser_settings = {0};

    parser_settings.on_header_field = upgrade_post_on_field_callback;
    parser_settings.on_header_value = upgrade_post_on_data_callback;

    multipart_parser *parser = multipart_parser_init(content_type, &parser_settings);


    return ESP_OK;

free_header_send_400:
    free(content_type);

    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;

send_500:
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"

/* cJSON */
#include "cJSON.h"

/* Base64 */
#include "mbedtls/base64.h"

/* App */
#include "app/api/config/handler_upgrade.h"
#include "app/version_manager.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define APP_OTA_MAXIMUM_BODY_SIZE (1536)

#define APP_OTA_SESSION_LEN (8)
#define APP_OTA_DATA_LEN    (1024)

typedef enum {
    APP_OTA_STATUS_IDLE        = 0U,
    APP_OTA_STATUS_IN_PROGRESS = 1U,
} app_api_config_upgrade_ota_status_t;

typedef enum {
    APP_OTA_ACTION_START  = 0U,
    APP_OTA_ACTION_WRITE  = 1U,
    APP_OTA_ACTION_COMMIT = 2U,
    APP_OTA_ACTION_ABORT  = 3U,
} app_api_config_upgrade_ota_action_t;

typedef struct {
    app_api_config_upgrade_ota_action_t action;
    char                                session[APP_OTA_SESSION_LEN + 1];
    struct {
        size_t  id;
        size_t  len;
        uint8_t data[APP_OTA_DATA_LEN];
    } payload;
} app_api_config_upgrade_ota_packet_t;

typedef struct {
    struct {
        app_api_config_upgrade_ota_status_t status;
        char                                session[APP_OTA_SESSION_LEN + 1];
        size_t                              packet_id;
    } ota;
} app_api_config_upgrade_state_t;

static const char *LOG_TAG = "asuna_httpupd";

static const char *s_upgrade_version_state_str[] = {
    [APP_OTA_SLOT_STATE_EMPTY]   = "empty",
    [APP_OTA_SLOT_STATE_READY]   = "ready",
    [APP_OTA_SLOT_STATE_IN_USE]  = "in_use",
    [APP_OTA_SLOT_STATE_INVALID] = "invalid",
};

static app_api_config_upgrade_state_t s_upgrade_state = {
    .ota =
        {
            .status = APP_OTA_STATUS_IDLE,
        },
};

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

        cJSON *state = cJSON_CreateString(s_upgrade_version_state_str[versions[i].state]);
        if (state == NULL) goto del_root_exit;
        cJSON_AddItemToObject(version, "state", state);

        if (versions[i].state == APP_OTA_SLOT_STATE_EMPTY) continue;
        /* The following entries only exists for a valid partition */

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

    cJSON *ota = cJSON_CreateObject();
    if (ota == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "ota", ota);

    cJSON *ota_status = cJSON_CreateNumber(s_upgrade_state.ota.status);
    if (ota_status == NULL) goto del_root_exit;
    cJSON_AddItemToObject(ota, "status", ota_status);

    if (s_upgrade_state.ota.status != APP_OTA_STATUS_IDLE) {
        cJSON *ota_session = cJSON_CreateString(s_upgrade_state.ota.session);
        if (ota_session == NULL) goto del_root_exit;
        cJSON_AddItemToObject(ota, "session", ota_session);
    }

    ret = cJSON_PrintUnformatted(root);

del_root_exit:
    cJSON_Delete(root);
    return ret;
}

static app_api_config_upgrade_ota_packet_t *app_api_config_handler_upgrade_packet_deserialize(const char *json) {
    char *buf = NULL;

    app_api_config_upgrade_ota_packet_t *pkt = malloc(sizeof(app_api_config_upgrade_ota_packet_t));
    if (pkt == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to allocate OTA packet.");
        return NULL;
    }

    cJSON *j = cJSON_Parse(json);
    if (j == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to parse OTA JSON payload.");
        goto free_pkt_exit;
    }

    cJSON *root_action = cJSON_GetObjectItem(j, "action");
    if (cJSON_IsInvalid(root_action)) goto del_json_exit;

    pkt->action = cJSON_GetNumberValue(root_action);

    /* Session ticket is optional when action is START */
    if (pkt->action != APP_OTA_ACTION_START) {
        cJSON *root_sesion = cJSON_GetObjectItem(j, "session");
        if (cJSON_IsInvalid(root_sesion)) goto del_json_exit;

        buf = cJSON_GetStringValue(root_sesion);
        if (buf == NULL) goto del_json_exit;

        strncpy(pkt->session, buf, sizeof(pkt->session));
    }

    /* Payload is optional when action is not WRITE */
    if (pkt->action == APP_OTA_ACTION_WRITE) {
        cJSON *root_payload = cJSON_GetObjectItem(j, "payload");
        if (cJSON_IsInvalid(root_payload) || !cJSON_IsObject(root_payload)) goto del_json_exit;

        cJSON *root_payload_id = cJSON_GetObjectItem(root_payload, "id");
        if (cJSON_IsInvalid(root_payload_id)) goto del_json_exit;

        pkt->payload.id = (size_t)cJSON_GetNumberValue(root_payload_id);

        cJSON *root_payload_data = cJSON_GetObjectItem(root_payload, "data");
        if (cJSON_IsInvalid(root_payload_data)) goto del_json_exit;

        buf = cJSON_GetStringValue(root_payload_data);
        if (buf == NULL) goto del_json_exit;

        int ret = mbedtls_base64_decode(pkt->payload.data, sizeof(pkt->payload.data), &pkt->payload.len,
                                        (const uint8_t *)buf, strlen(buf));
        if (ret != 0) goto del_json_exit;
    }

    cJSON_Delete(j);

    return pkt;

del_json_exit:
    cJSON_Delete(j);

free_pkt_exit:
    free(pkt);

    return NULL;
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

static esp_err_t app_api_config_handler_upgrade_post(httpd_req_t *req) {
    size_t payload_size = req->content_len;
    if (payload_size > APP_OTA_MAXIMUM_BODY_SIZE) {
        goto send_413;
    }

    char *payload = malloc(payload_size);
    if (payload == NULL) goto send_500;

    int ret = httpd_req_recv(req, payload, payload_size);
    if (ret <= 0) goto free_buf_send_500;

    app_api_config_upgrade_ota_packet_t *packet = app_api_config_handler_upgrade_packet_deserialize(payload);
    if (packet == NULL) {
        ESP_LOGW(LOG_TAG, "Failed to deserialize packet.");
        goto free_buf_send_400;
    }

    char resp[128];

    switch (packet->action) {
        case APP_OTA_ACTION_START: {
            if (s_upgrade_state.ota.status != APP_OTA_STATUS_IDLE) {
                goto free_pkt_send_400;
            }

            snprintf(s_upgrade_state.ota.session, sizeof(s_upgrade_state.ota.session), "%08lx", esp_random());
            if (app_version_manager_ota_start() != 0) {
                goto free_pkt_send_500;
            }

            s_upgrade_state.ota.status    = APP_OTA_STATUS_IN_PROGRESS;
            s_upgrade_state.ota.packet_id = 0;

            snprintf(resp, sizeof(resp), "{\"status\": \"success\", \"session\": \"%s\"}", s_upgrade_state.ota.session);
            break;
        }

        case APP_OTA_ACTION_WRITE: {
            if (s_upgrade_state.ota.status != APP_OTA_STATUS_IN_PROGRESS) {
                ESP_LOGW(LOG_TAG, "OTA session is not started.");
                goto free_pkt_send_400;
            }

            if (strcmp(s_upgrade_state.ota.session, packet->session) != 0) {
                ESP_LOGW(LOG_TAG, "Not our OTA session.");
                goto free_pkt_send_400;
            }

            if (s_upgrade_state.ota.packet_id != packet->payload.id) {
                ESP_LOGW(LOG_TAG, "Out-of-order OTA packet.");
                goto free_pkt_send_400;
            }

            if (app_version_manager_ota_save(packet->payload.data, packet->payload.len) != 0) {
                ESP_LOGE(LOG_TAG, "OTA payload write failed.");
                goto free_pkt_send_500;
            }

            s_upgrade_state.ota.packet_id++;

            snprintf(resp, sizeof(resp), "{\"status\": \"success\"}");
            break;
        }

        case APP_OTA_ACTION_COMMIT: {
            if (s_upgrade_state.ota.status != APP_OTA_STATUS_IN_PROGRESS) {
                ESP_LOGW(LOG_TAG, "OTA session is not started");
                goto free_pkt_send_400;
            }

            if (strcmp(s_upgrade_state.ota.session, packet->session) != 0) {
                ESP_LOGW(LOG_TAG, "Not our OTA session.");
                goto free_pkt_send_400;
            }

            if (app_version_manager_ota_commit() != 0) {
                ESP_LOGE(LOG_TAG, "OTA commit failed.");
                goto free_pkt_send_500;
            }

            s_upgrade_state.ota.status = APP_OTA_STATUS_IDLE;

            snprintf(resp, sizeof(resp), "{\"status\": \"success\"}");
            break;
        }

        case APP_OTA_ACTION_ABORT: {
            if (s_upgrade_state.ota.status != APP_OTA_STATUS_IDLE) {
                s_upgrade_state.ota.status = APP_OTA_STATUS_IDLE;
                app_version_manager_ota_abort();
            }

            snprintf(resp, sizeof(resp), "{\"status\": \"success\"}");
            break;
        }

        default:
            break;
    }

    free(packet);
    free(payload);

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;

free_pkt_send_400:
    free(packet);

free_buf_send_400:
    free(payload);

    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;

free_pkt_send_500:
    free(packet);

free_buf_send_500:
    free(payload);

send_500:
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;

send_413:
    httpd_resp_set_status(req, "413 Payload Too Large");
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

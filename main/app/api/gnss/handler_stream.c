#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"

/* cJSON */
#include "cJSON.h"

/* App */
#include "app/api/gnss/handler_stream.h"

typedef struct app_ws_client_s {
    httpd_handle_t handle;

    struct app_ws_client_s *next;
} app_ws_client_t;

static const char *LOG_TAG = "A_HS";

static app_ws_client_t *s_app_ws_client_list = NULL;

static int app_ws_client_list_add(httpd_handle_t handle, app_ws_client_t *client) {
    if (s_app_ws_client_list == NULL) {
        s_app_ws_client_list = malloc(sizeof(app_ws_client_t));
        if (s_app_ws_client_list == NULL) {
            ESP_LOGE(LOG_TAG, "Failed to allocate WS client item.");
            return -1;
        }
    }

    return 0;
}

static esp_err_t app_api_gnss_handler_stream_transfer(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGD(LOG_TAG, "New stream client connected.");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_packet;
    uint8_t         *ws_payload = NULL;

    memset(&ws_packet, 0U, sizeof(ws_packet));

    ws_packet.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_packet, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to get frame length.");

        return ret;
    }

    if (ws_packet.len > 0) {
        ws_payload = malloc(ws_packet.len);
        if (ws_payload == NULL) {
            ESP_LOGE(LOG_TAG, "Failed to allocate message payload.");

            return ESP_ERR_NO_MEM;
        }

        ret = httpd_ws_recv_frame(req, &ws_packet, ws_packet.len);
        if (ret != ESP_OK) {
            ESP_LOGE(LOG_TAG, "Failed to receive ws frame.");

            goto free_rxbuf_exit;
        }

        ESP_LOGD(LOG_TAG, "Frame received, length: %d", ws_packet.len);
    }

    ESP_LOGD(LOG_TAG, "WebSocket frame type: %d", ws_packet.type);

    switch (ws_packet.type) {
        case HTTPD_WS_TYPE_TEXT:
            /* TODO: Handle incoming commands */
            break;

        default:
            break;
    }

    ws_packet.payload = "{}";

    ret = httpd_ws_send_frame(req, &ws_packet);
    if (ret != ESP_OK) {
        goto free_rxbuf_exit;
    }

free_rxbuf_exit:
    if (ws_payload != NULL) {
        free(ws_payload);
    }

    return ret;
}

const httpd_uri_t app_api_gnss_handler_stream_ws_uri = {
    .uri          = "/api/gnss/stream",
    .method       = HTTP_GET,
    .handler      = app_api_gnss_handler_stream_transfer,
    .user_ctx     = NULL,
    .is_websocket = true,
};

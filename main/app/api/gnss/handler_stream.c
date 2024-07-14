#include "freertos/FreeRTOS.h"
#include "freertos/list.h"
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
} app_ws_client_t;

static const char *LOG_TAG = "asuna_gstream";

static List_t s_app_ws_client_list;

static int app_ws_client_list_add(httpd_handle_t handle) {
    ListItem_t *item = malloc(sizeof(ListItem_t));
    if (item == NULL) {
        return -1;
    }

    vListInitialiseItem(item);

    app_ws_client_t *client = malloc(sizeof(app_ws_client_t));
    if (client == NULL) {
        free(item);

        return -2;
    }

    client->handle = handle;

    item->xItemValue = (TickType_t)client;

    vListInsert(&s_app_ws_client_list, item);

    ESP_LOGI(LOG_TAG, "New stream client connected, handle=%p", handle);

    return 0;
}

static int app_ws_client_list_remove(httpd_handle_t handle) {
    bool removed_item = false;

    ListItem_t       *item = listGET_HEAD_ENTRY(&s_app_ws_client_list);
    const ListItem_t *end  = listGET_END_MARKER(&s_app_ws_client_list);

    while (item != NULL) {
        if (item == end) {
            break;
        }

        app_ws_client_t *client = (app_ws_client_t *)item->xItemValue;

        if (client->handle == handle) {
            listREMOVE_ITEM(item);
            free(client);
            free(item);

            removed_item = true;
            break;
        }

        item = listGET_NEXT(item);
    }

    if (removed_item) {
        ESP_LOGI(LOG_TAG, "Successfully removed stream client from list, handle=%p", handle);
    } else {
        ESP_LOGI(LOG_TAG, "No matching item removed from the list, handle=%p", handle);
    }

    return 0;
}

static esp_err_t app_api_gnss_handler_stream_transfer(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        app_ws_client_list_add(req->handle);

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

        ws_packet.payload = ws_payload;

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

int app_api_gnss_handler_stream_ws_init(void) {
    vListInitialise(&s_app_ws_client_list);

    /* TODO: Register GNSS event callback here to dispatch to all clients. */
    /* Note: callbacks are running in the GNSS parser task. */

    return 0;
}

int app_api_gnss_handler_stream_ws_onopen(httpd_handle_t handle, int fd) {
    ESP_LOGD(LOG_TAG, "Socket onOpen(), fd=%d", fd);

    return 0;
}

int app_api_gnss_handler_stream_ws_onclose(httpd_handle_t handle, int fd) {
    ESP_LOGD(LOG_TAG, "Socket onClose(), fd=%d", fd);

    /* Note: The handle may not belong to us. */
    app_ws_client_list_remove(handle);

    return 0;
}

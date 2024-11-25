#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"

/* cJSON */
#include "cJSON.h"

/* App */
#include "app/api/status/handler_network.h"
#include "app/netif_common.h"

static int app_api_status_handler_network_serialize_if(const app_netif_if_status_t *if_status, cJSON *root) {
    cJSON *root_valid = cJSON_CreateBool(if_status->valid);
    if (root_valid == NULL) goto exit_err;
    cJSON_AddItemToObject(root, "valid", root_valid);

    if (if_status->valid) {
        cJSON *root_ipv4 = cJSON_CreateObject();
        if (root_ipv4 == NULL) goto exit_err;
        cJSON_AddItemToObject(root, "ipv4", root_ipv4);

        cJSON *root_ipv4_addr = cJSON_CreateString(if_status->ipv4.addr);
        if (root_ipv4_addr == NULL) goto exit_err;
        cJSON_AddItemToObject(root_ipv4, "addr", root_ipv4_addr);

        cJSON *root_ipv4_netmask = cJSON_CreateString(if_status->ipv4.netmask);
        if (root_ipv4_netmask == NULL) goto exit_err;
        cJSON_AddItemToObject(root_ipv4, "netmask", root_ipv4_netmask);

        cJSON *root_ipv4_gateway = cJSON_CreateString(if_status->ipv4.gateway);
        if (root_ipv4_gateway == NULL) goto exit_err;
        cJSON_AddItemToObject(root_ipv4, "gateway", root_ipv4_gateway);

        cJSON *root_ipv4_dns = cJSON_CreateObject();
        if (root_ipv4_dns == NULL) goto exit_err;
        cJSON_AddItemToObject(root_ipv4, "dns", root_ipv4_dns);

        cJSON *root_ipv4_dns_main = cJSON_CreateString(if_status->ipv4.dns_main);
        if (root_ipv4_dns_main == NULL) goto exit_err;
        cJSON_AddItemToObject(root_ipv4_dns, "main", root_ipv4_dns_main);

        cJSON *root_ipv4_dns_backup = cJSON_CreateString(if_status->ipv4.dns_back);
        if (root_ipv4_dns_backup == NULL) goto exit_err;
        cJSON_AddItemToObject(root_ipv4_dns, "backup", root_ipv4_dns_backup);
    }

    return 0;

exit_err:
    return -1;
}

static char *app_api_status_handler_network_serialize(const app_netif_status_t *status) {
    char *ret = NULL;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON *root_sta = cJSON_CreateObject();
    if (root_sta == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "sta", root_sta);

    if (app_api_status_handler_network_serialize_if(&status->wifi_sta, root_sta) != 0) goto del_root_exit;

    cJSON *root_ap = cJSON_CreateObject();
    if (root_ap == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "ap", root_ap);

    if (app_api_status_handler_network_serialize_if(&status->wifi_ap, root_ap) != 0) goto del_root_exit;

    cJSON *root_lte = cJSON_CreateObject();
    if (root_lte == NULL) goto del_root_exit;
    cJSON_AddItemToObject(root, "lte", root_lte);

    if (app_api_status_handler_network_serialize_if(&status->lte, root_lte) != 0) goto del_root_exit;

    ret = cJSON_PrintUnformatted(root);

del_root_exit:
    cJSON_Delete(root);
    return ret;
}

static esp_err_t app_api_status_handler_network_get(httpd_req_t *req) {
    app_netif_status_t netif_status;

    if (app_netif_get_status(&netif_status) != 0) {
        goto send_500;
    }

    char *json = app_api_status_handler_network_serialize(&netif_status);
    if (json == NULL) goto send_500;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    cJSON_free(json);

send_500:
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
}

const httpd_uri_t app_api_status_handler_network_get_uri = {
    .uri      = "/api/status/network",
    .method   = HTTP_GET,
    .handler  = app_api_status_handler_network_get,
    .user_ctx = NULL,
};
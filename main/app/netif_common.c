/* IDF */
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

/* App */
#include <string.h>

#include "app/netif_common.h"

static const char *LOG_TAG = "asuna_net";

static esp_netif_t *s_app_netif_list[APP_NETIF_END];

static void app_netif_ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

int app_netif_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, app_netif_ip_event_cb, NULL));

    ESP_LOGI(LOG_TAG, "Network interface initialization completed.");

    return 0;
}

int app_netif_register(app_netif_type_t type, void *netif) {
    s_app_netif_list[type] = (esp_netif_t *)netif;
    return 0;
}

int app_netif_get_if_status(app_netif_type_t type, app_netif_if_status_t *status) {
    esp_netif_t *netif = s_app_netif_list[type];

    memset(status, 0, sizeof(app_netif_if_status_t));

    if (netif == NULL) {
        return 0;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return -1;
    }

    esp_netif_dns_info_t dns_info_main;
    esp_netif_dns_info_t dns_info_back;
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info_main) != ESP_OK) {
        return -2;
    }

    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info_back) != ESP_OK) {
        return -3;
    }

    status->valid = true;

    snprintf(status->ipv4.addr, sizeof(status->ipv4.addr), IPSTR, IP2STR(&ip_info.ip));
    snprintf(status->ipv4.netmask, sizeof(status->ipv4.netmask), IPSTR, IP2STR(&ip_info.netmask));
    snprintf(status->ipv4.gateway, sizeof(status->ipv4.gateway), IPSTR, IP2STR(&ip_info.gw));
    snprintf(status->ipv4.dns_main, sizeof(status->ipv4.dns_main), IPSTR, IP2STR(&dns_info_main.ip.u_addr.ip4));
    snprintf(status->ipv4.dns_back, sizeof(status->ipv4.dns_back), IPSTR, IP2STR(&dns_info_back.ip.u_addr.ip4));

    return 0;
}

int app_netif_get_status(app_netif_status_t *status) {
    int ret = 0;

    ret = app_netif_get_if_status(APP_NETIF_WIFI_STA, &status->wifi_sta);
    if (ret != 0) return ret;

    ret = app_netif_get_if_status(APP_NETIF_WIFI_AP, &status->wifi_ap);
    if (ret != 0) return ret;

    ret = app_netif_get_if_status(APP_NETIF_LTE, &status->lte);
    return ret;
}

static void app_netif_ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGD(LOG_TAG, "IP event! %" PRIu32, event_id);

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            const ip_event_got_ip_t *event = event_data;
            esp_netif_t             *netif = event->esp_netif;
            esp_netif_dns_info_t     dns_info;

            ESP_LOGI(LOG_TAG, "WiFi STA IP event: Acquired");

            ESP_LOGI(LOG_TAG, "IP     : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(LOG_TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(LOG_TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));

            esp_netif_get_dns_info(netif, 0, &dns_info);
            ESP_LOGI(LOG_TAG, "NS1    : " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));

            esp_netif_get_dns_info(netif, 1, &dns_info);
            ESP_LOGI(LOG_TAG, "NS2    : " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));

            break;
        }

        case IP_EVENT_STA_LOST_IP: {
            ESP_LOGI(LOG_TAG, "WiFi STA IP event: Lost");

            break;
        }

        case IP_EVENT_GOT_IP6: {
            const ip_event_got_ip6_t *event = event_data;

            ESP_LOGI(LOG_TAG, "IPv6 event: Acquired");
            ESP_LOGI(LOG_TAG, "IPv6: " IPV6STR, IPV62STR(event->ip6_info.ip));

            break;
        }

        default:
            break;
    }
}

/* IDF */
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

/* App */
#include "app/netif_common.h"

static const char *LOG_TAG = "asuna_net";

static void app_netif_ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

int app_netif_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, app_netif_ip_event_cb, NULL));

    ESP_LOGI(LOG_TAG, "Network interface initialization completed.");

    return 0;
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

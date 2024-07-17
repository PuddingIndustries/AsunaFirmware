/* IDF */
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "linenoise/linenoise.h"
#include "nvs.h"
#include "nvs_flash.h"

/* App */
#include "app/console/cmd_ip.h"

#define APP_CONSOLE_IP_MAX_INTERFACES 32

static bool app_console_iterate_interfaces(esp_netif_t *netif, void *ctx) {
    esp_netif_t **iface_list = ctx;

    size_t idx;
    for (idx = 0; idx < APP_CONSOLE_IP_MAX_INTERFACES; idx++) {
        if (iface_list[idx] == netif) {
            break;
        }

        if (iface_list[idx] == NULL) {
            break;
        }
    }

    iface_list[idx] = netif;

    return false;
}

static int app_console_ip_print_iface(esp_netif_t *iface) {
    printf("Interface %s(%p): \n", esp_netif_get_desc(iface), iface);

    uint8_t mac_addr[6];

    if (esp_netif_get_mac(iface, mac_addr) != ESP_OK) {
        return -1;
    }

    printf("\tMAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(iface, &ip_info) != ESP_OK) {
        return -2;
    }

    printf("\t       IPv4: " IPSTR "\n", IP2STR(&ip_info.ip));
    printf("\t    Netmask: " IPSTR "\n", IP2STR(&ip_info.netmask));
    printf("\t    Gateway: " IPSTR "\n", IP2STR(&ip_info.gw));

    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(iface, ESP_NETIF_DNS_MAIN, &dns_info);
    printf("\tPrimary DNS: " IPSTR "\n", IP2STR(&dns_info.ip.u_addr.ip4));

    esp_netif_get_dns_info(iface, ESP_NETIF_DNS_BACKUP, &dns_info);
    printf("\t Backup DNS: " IPSTR "\n", IP2STR(&dns_info.ip.u_addr.ip4));

    return 0;
}

static int app_console_ip_func(int argc, char **argv) {
    esp_netif_t *iface_list[APP_CONSOLE_IP_MAX_INTERFACES] = {NULL};

    esp_netif_find_if(app_console_iterate_interfaces, iface_list);

    size_t idx = 0U;
    while (iface_list[idx] != NULL) {
        esp_netif_t *iface = iface_list[idx];

        int ret = app_console_ip_print_iface(iface);
        if (ret != 0) {
            printf("Failed to get information for network interface %p\n", iface);
        }

        printf("\n");

        idx++;
    }

    return 0;
}

const esp_console_cmd_t app_console_cmd_ip = {
    .command = "ip",
    .help    = "Get system network interface information",
    .hint    = NULL,
    .func    = app_console_ip_func,
};
#ifndef APP_NETIF_COMMON_H
#define APP_NETIF_COMMON_H

typedef enum {
    APP_NETIF_WIFI_STA,
    APP_NETIF_WIFI_AP,
    APP_NETIF_LTE,
    APP_NETIF_END,
} app_netif_type_t;

typedef struct {
    char addr[16];
    char netmask[16];
    char gateway[16];
    char dns_main[16];
    char dns_back[16];
} app_netif_ip4_status_t;

typedef struct {
} app_netif_ip6_status_t;

typedef struct {
    bool                   valid;
    app_netif_ip4_status_t ipv4;
    app_netif_ip6_status_t ipv6;
} app_netif_if_status_t;

typedef struct {
    app_netif_if_status_t wifi_sta;
    app_netif_if_status_t wifi_ap;
    app_netif_if_status_t lte;
} app_netif_status_t;

int app_netif_init(void);
int app_netif_register(app_netif_type_t type, void *netif);
int app_netif_get_if_status(app_netif_type_t type, app_netif_if_status_t *status);
int app_netif_get_status(app_netif_status_t *status);

#endif  // APP_NETIF_COMMON_H

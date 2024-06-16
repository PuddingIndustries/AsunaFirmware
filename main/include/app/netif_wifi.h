#ifndef APP_NETIF_WIFI_H
#define APP_NETIF_WIFI_H

#include <stdint.h>

typedef struct {
    char    ssid[32];
    char    pass[64];
    uint8_t chan;
} app_netif_wifi_ap_config_t;

typedef struct {
    char ssid[32];
    char pass[64];
} app_netif_wifi_sta_config_t;

typedef struct {
    uint8_t ap_enabled;
    uint8_t sta_enabled;

    app_netif_wifi_ap_config_t  ap_config;
    app_netif_wifi_sta_config_t sta_config;
} app_netif_wifi_config_t;

int  app_netif_wifi_init(void);
int  app_netif_wifi_config_get(app_netif_wifi_config_t *config);
int  app_netif_wifi_config_set(const app_netif_wifi_config_t *config);
void app_netif_wifi_config_init(app_netif_wifi_config_t *config);
int  app_netif_wifi_config_reload(void);
int  app_netif_wifi_sta_reconnect(void);

#endif  // APP_NETIF_WIFI_H

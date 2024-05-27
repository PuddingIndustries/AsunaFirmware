#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* IDF */
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

/* App */
#include "app/netif_wifi.h"

#define APP_NETIF_WIFI_NVS_NAMESPACE "a_netif_wifi"

#define APP_NETIF_WIFI_AP_SSID_PREFIX  "ASUNA_"
#define APP_NETIF_WIFI_AP_DEFAULT_CHAN 1
#define APP_NETIF_WIFI_AP_MAX_CLIENTS  8
#define APP_NETIF_WIFI_AP_SUPPORT_WPA3 1

static const char *APP_NETIF_WIFI_CFG_KEY_AP_CHAN = "ap_chan";
static const char *APP_NETIF_WIFI_CFG_KEY_AP_SSID = "ap_ssid";
static const char *APP_NETIF_WIFI_CFG_KEY_AP_PASS = "ap_pass";

static const char *LOG_TAG = "a_wifi";

static void app_netif_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

int app_netif_wifi_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing...");

    wifi_init_config_t           cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_event_handler_instance_t instance_any_id;

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &app_netif_wifi_event_handler,
                                                        NULL, &instance_any_id));

    nvs_handle_t wifi_cfg_handle;
    ESP_ERROR_CHECK(nvs_open(APP_NETIF_WIFI_NVS_NAMESPACE, NVS_READWRITE, &wifi_cfg_handle));

    char    ap_ssid[32];
    char    ap_pass[64];
    uint8_t ap_chan;

    size_t ap_ssid_len = 32;
    size_t ap_pass_len = 64;

    /* ---- Load / Generate Channel ---- */
    if (nvs_get_u8(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_CHAN, &ap_chan) != ESP_OK) {
        ESP_LOGW(LOG_TAG, "Setting channel to default (%d)", APP_NETIF_WIFI_AP_DEFAULT_CHAN);
        ESP_ERROR_CHECK(nvs_set_u8(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_CHAN, APP_NETIF_WIFI_AP_DEFAULT_CHAN));
    }

    ESP_ERROR_CHECK(nvs_get_u8(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_CHAN, &ap_chan));

    /* ---- Load / Generate SSID based on AP MAC ---- */

    if (nvs_get_str(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_SSID, NULL, &ap_ssid_len) != ESP_OK) {
        uint8_t ap_mac[6];

        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, ap_mac));

        snprintf(ap_ssid, 32, APP_NETIF_WIFI_AP_SSID_PREFIX "%02X%02X%02X", ap_mac[3], ap_mac[4], ap_mac[5]);
        ESP_LOGW(LOG_TAG, "Creating default AP SSID: %s", ap_ssid);

        ESP_ERROR_CHECK(nvs_set_str(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_SSID, ap_ssid));
    }

    ESP_ERROR_CHECK(nvs_get_str(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_SSID, ap_ssid, &ap_ssid_len));

    /* ---- Load / Generate Password ---- */

    if (nvs_get_str(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_PASS, NULL, &ap_pass_len) != ESP_OK) {
        uint32_t rand_pass = esp_random();
        uint8_t  pass_buf[4];

        pass_buf[0] = (rand_pass >> 24U) & 0xFFU;
        pass_buf[1] = (rand_pass >> 16U) & 0xFFU;
        pass_buf[2] = (rand_pass >> 8U) & 0xFFU;
        pass_buf[3] = (rand_pass >> 0U) & 0xFFU;

        snprintf(ap_pass, 64, "%02x%02x%02x%02x", pass_buf[0], pass_buf[1], pass_buf[2], pass_buf[3]);
        ESP_LOGW(LOG_TAG, "Creating default AP password: %s", ap_pass);

        ESP_ERROR_CHECK(nvs_set_str(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_PASS, ap_pass));
    }

    ESP_ERROR_CHECK(nvs_get_str(wifi_cfg_handle, APP_NETIF_WIFI_CFG_KEY_AP_PASS, ap_pass, &ap_pass_len));

    /* ---- Commit configuration changes ---- */
    nvs_commit(wifi_cfg_handle);
    nvs_close(wifi_cfg_handle);

    /* ---- Initialize WiFi drivers ---- */

    wifi_config_t wifi_config = {
        .ap =
            {
#if APP_NETIF_WIFI_AP_SUPPORT_WPA3
                .authmode    = WIFI_AUTH_WPA3_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else
                .authmode = WIFI_AUTH_WPA2_PSK,
#endif
                .channel        = ap_chan,
                .max_connection = APP_NETIF_WIFI_AP_MAX_CLIENTS,
                .pmf_cfg        = {.capable = true, .required = true},
            },
    };

    strncpy((char *)wifi_config.ap.ssid, ap_ssid, 32);
    strncpy((char *)wifi_config.ap.password, ap_pass, 64);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return 0;
}

static void app_netif_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED: {
            const wifi_event_ap_staconnected_t *event = event_data;
            ESP_LOGI(LOG_TAG, "Station " MACSTR " joined, AID=%d.", MAC2STR(event->mac), event->aid);

            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            const wifi_event_ap_stadisconnected_t *event = event_data;
            ESP_LOGI(LOG_TAG, "Station " MACSTR " left, AID=%d.", MAC2STR(event->mac), event->aid);

            break;
        }
        default:
            break;
    }
}
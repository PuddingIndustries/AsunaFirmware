#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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
#define APP_NETIF_WIFI_NVS_VERSION   1 /* DO NOT CHANGE THIS VALUE UNLESS THERE IS A STRUCTURE UPDATE */

#define APP_NETIF_WIFI_AP_SSID_PREFIX  "ASUNA_"
#define APP_NETIF_WIFI_AP_DEFAULT_CHAN 1
#define APP_NETIF_WIFI_AP_MAX_CLIENTS  8
#define APP_NETIF_WIFI_AP_SUPPORT_WPA3 1

#define APP_NETIF_WIFI_STA_DEFAULT_SSID "SSID"
#define APP_NETIF_WIFI_STA_DEFAULT_PASS "PASSWORD"
#define APP_NETIF_WIFI_STA_AUTH_THRESH  WIFI_AUTH_OPEN
#define APP_NETIF_WIFI_STA_MAX_RETRIES  10

typedef enum {
    APP_NETIF_WIFI_MGR_CFG_RELOAD,
    APP_NETIF_WIFI_MGR_STA_RECONNECT,
} app_netif_wifi_queue_type_t;

typedef struct {
    app_netif_wifi_queue_type_t type;
    void                       *parameter;
} app_netif_wifi_queue_item_t;

static const char *APP_NETIF_WIFI_CFG_KEY_FLAG        = "cfg_valid";
static const char *APP_NETIF_WIFI_CFG_KEY_AP_ENABLED  = "ap_enabled";
static const char *APP_NETIF_WIFI_CFG_KEY_AP_CHAN     = "ap_chan";
static const char *APP_NETIF_WIFI_CFG_KEY_AP_SSID     = "ap_ssid";
static const char *APP_NETIF_WIFI_CFG_KEY_AP_PASS     = "ap_pass";
static const char *APP_NETIF_WIFI_CFG_KEY_STA_ENABLED = "sta_enabled";
static const char *APP_NETIF_WIFI_CFG_KEY_STA_SSID    = "sta_ssid";
static const char *APP_NETIF_WIFI_CFG_KEY_STA_PASS    = "sta_pass";

static const char *LOG_TAG = "asuna_wifi";

static SemaphoreHandle_t s_netif_wifi_cfg_semphr = NULL;
static QueueHandle_t     s_netif_wifi_mgr_queue  = NULL;

static int  app_netif_wifi_driver_init(const app_netif_wifi_config_t *app_cfg);
static int  app_netif_wifi_driver_deinit(void);
static void app_netif_wifi_manager_task(void *arguments);
static void app_netif_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

int app_netif_wifi_init(void) {
    int ret = 0;

    ESP_LOGI(LOG_TAG, "Initializing...");

    const wifi_init_config_t     cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_event_handler_instance_t instance_any_id;

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &app_netif_wifi_event_handler,
                                                        NULL, &instance_any_id));

    s_netif_wifi_cfg_semphr = xSemaphoreCreateBinary();
    if (s_netif_wifi_cfg_semphr == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create WiFi config semaphore.");
        return -1;
    }

    xSemaphoreGive(s_netif_wifi_cfg_semphr);

    s_netif_wifi_mgr_queue = xQueueCreate(4, sizeof(app_netif_wifi_queue_item_t));
    if (s_netif_wifi_mgr_queue == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create WiFi manager queue.");
        goto deinit_semphr_exit;
    }

    app_netif_wifi_config_t app_cfg;
    if (app_netif_wifi_config_get(&app_cfg) != 0) {
        ESP_LOGW(LOG_TAG, "Configuration invalid, restore to default...");
        app_netif_wifi_config_init(&app_cfg);

        if (app_netif_wifi_config_set(&app_cfg) != 0) {
            ESP_LOGE(LOG_TAG, "Configuration validation failed...");
            ret = -2;

            goto deinit_queue_exit;
        }

        ESP_LOGW(LOG_TAG, "Default AP SSID: %s, Password: %s", app_cfg.ap_config.ssid, app_cfg.ap_config.pass);
    }

    /* ---- Initialize WiFi drivers ---- */
    if (app_netif_wifi_driver_init(&app_cfg) != 0) {
        ESP_LOGE(LOG_TAG, "Failed to initialize WiFi driver.");

        ret = -3;
        goto deinit_queue_exit;
    }

    if (xTaskCreate(app_netif_wifi_manager_task, "WIFI_MGR", 2048, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to initialize WiFi manager task.");

        ret = -4;
        goto deinit_wifi_exit;
    }

    return ret;

deinit_wifi_exit:
    app_netif_wifi_driver_deinit();

deinit_queue_exit:
    vQueueDelete(s_netif_wifi_mgr_queue);

deinit_semphr_exit:
    vSemaphoreDelete(s_netif_wifi_cfg_semphr);

    return ret;
}

int app_netif_wifi_config_get(app_netif_wifi_config_t *config) {
    int       ret = 0;
    esp_err_t err;

    if (xSemaphoreTake(s_netif_wifi_cfg_semphr, portMAX_DELAY) != pdPASS) {
        return -1;
    }
    /* ---- Create NVS handle ---- */
    nvs_handle_t handle;
    err = nvs_open(APP_NETIF_WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ret = -1;
        goto release_lock_exit;
    }

    ESP_ERROR_CHECK(err);

    /* ---- Check NVS data flag ---- */

    uint8_t cfg_flag;
    if (nvs_get_u8(handle, APP_NETIF_WIFI_CFG_KEY_FLAG, &cfg_flag) != ESP_OK) {
        ret = -1;
        goto release_lock_exit;
    }

    /* ?? Downgrade is not allowed ?? */
    if (cfg_flag > APP_NETIF_WIFI_NVS_VERSION) {
        ret = -2;
        goto release_lock_exit;
    }

    /* TODO: Handle structure update. */

    /* ---- Load configuration: ap_enabled ---- */
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_NETIF_WIFI_CFG_KEY_AP_ENABLED, &config->ap_enabled));

    /* ---- Load configuration: sta_enabled ---- */
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_NETIF_WIFI_CFG_KEY_STA_ENABLED, &config->sta_enabled));

    /* ---- Load configuration: ap_config.ssid ---- */
    size_t str_len;
    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_AP_SSID, NULL, &str_len));
    if (str_len > sizeof(config->ap_config.ssid)) {
        ret = -3;
        goto close_exit;
    }

    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_AP_SSID, config->ap_config.ssid, &str_len));

    /* ---- Load configuration: ap_config.pass ---- */
    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_AP_PASS, NULL, &str_len));
    if (str_len > sizeof(config->ap_config.pass)) {
        ret = -3;
        goto close_exit;
    }

    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_AP_PASS, config->ap_config.pass, &str_len));

    /* ---- Load configuration: ap_config.chan ---- */
    ESP_ERROR_CHECK(nvs_get_u8(handle, APP_NETIF_WIFI_CFG_KEY_AP_CHAN, &config->ap_config.chan));

    /* ---- Load configuration: sta_config.ssid ---- */
    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_STA_SSID, NULL, &str_len));
    if (str_len > sizeof(config->sta_config.ssid)) {
        ret = -3;
        goto close_exit;
    }
    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_STA_SSID, config->sta_config.ssid, &str_len));

    /* ---- Load configuration: sta_config.pass ---- */
    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_STA_PASS, NULL, &str_len));
    if (str_len > sizeof(config->sta_config.pass)) {
        ret = -3;
        goto close_exit;
    }
    ESP_ERROR_CHECK(nvs_get_str(handle, APP_NETIF_WIFI_CFG_KEY_STA_PASS, config->sta_config.pass, &str_len));

/* ---- Close NVS handle ---- */
close_exit:
    nvs_close(handle);

release_lock_exit:
    xSemaphoreGive(s_netif_wifi_cfg_semphr);

    return ret;
}

int app_netif_wifi_config_set(const app_netif_wifi_config_t *config) {
    /* ---- Sanity checks ---- */
    if (config->ap_enabled) {
        if (config->ap_config.chan > 14 || config->ap_config.chan == 0) {
            return -1;
        }

        if (strnlen(config->ap_config.ssid, sizeof(config->ap_config.ssid)) < 4) {
            return -1;
        }

        if (strnlen(config->ap_config.pass, sizeof(config->ap_config.pass)) < 8) {
            return -1;
        }
    }

    if (config->sta_enabled) {
        if (strnlen(config->sta_config.pass, sizeof(config->sta_config.pass)) < 4) {
            return -2;
        }

        if (strnlen(config->sta_config.pass, sizeof(config->sta_config.pass)) < 8) {
            return -2;
        }
    }

    if (xSemaphoreTake(s_netif_wifi_cfg_semphr, portMAX_DELAY) != pdPASS) {
        return -3;
    }

    /* ---- Create NVS handle ---- */
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(APP_NETIF_WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle));

    /* ---- Store configuration ---- */
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_NETIF_WIFI_CFG_KEY_FLAG, APP_NETIF_WIFI_NVS_VERSION));

    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_NETIF_WIFI_CFG_KEY_AP_ENABLED, config->ap_enabled));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_NETIF_WIFI_CFG_KEY_STA_ENABLED, config->sta_enabled));

    ESP_ERROR_CHECK(nvs_set_str(handle, APP_NETIF_WIFI_CFG_KEY_AP_SSID, config->ap_config.ssid));
    ESP_ERROR_CHECK(nvs_set_str(handle, APP_NETIF_WIFI_CFG_KEY_AP_PASS, config->ap_config.pass));
    ESP_ERROR_CHECK(nvs_set_u8(handle, APP_NETIF_WIFI_CFG_KEY_AP_CHAN, config->ap_config.chan));

    ESP_ERROR_CHECK(nvs_set_str(handle, APP_NETIF_WIFI_CFG_KEY_STA_SSID, config->sta_config.ssid));
    ESP_ERROR_CHECK(nvs_set_str(handle, APP_NETIF_WIFI_CFG_KEY_STA_PASS, config->sta_config.pass));

    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);

    xSemaphoreGive(s_netif_wifi_cfg_semphr);

    return 0;
}

void app_netif_wifi_config_init(app_netif_wifi_config_t *config) {
    uint8_t ap_mac[6];

    config->ap_enabled     = 1U;
    config->sta_enabled    = 0U;
    config->ap_config.chan = APP_NETIF_WIFI_AP_DEFAULT_CHAN;

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, ap_mac));
    snprintf(config->ap_config.ssid, sizeof(config->ap_config.ssid), APP_NETIF_WIFI_AP_SSID_PREFIX "%02X%02X%02X",
             ap_mac[3], ap_mac[4], ap_mac[5]);

    snprintf(config->ap_config.pass, sizeof(config->ap_config.pass), "%02x%02x%02x%02x", ap_mac[2], ap_mac[3],
             ap_mac[4], ap_mac[5]);

    snprintf((char *)config->sta_config.ssid, sizeof(config->sta_config.ssid), APP_NETIF_WIFI_STA_DEFAULT_SSID);
    snprintf((char *)config->sta_config.pass, sizeof(config->sta_config.pass), APP_NETIF_WIFI_STA_DEFAULT_PASS);
}

int app_netif_wifi_config_reload(void) {
    app_netif_wifi_queue_item_t evt = {
        .type      = APP_NETIF_WIFI_MGR_CFG_RELOAD,
        .parameter = NULL,
    };

    if (xQueueSend(s_netif_wifi_mgr_queue, &evt, portMAX_DELAY) != pdPASS) {
        return -1;
    }

    return 0;
}

int app_netif_wifi_sta_reconnect(void) {
    app_netif_wifi_queue_item_t evt = {
        .type      = APP_NETIF_WIFI_MGR_STA_RECONNECT,
        .parameter = NULL,
    };

    if (xQueueSend(s_netif_wifi_mgr_queue, &evt, portMAX_DELAY) != pdPASS) {
        return -1;
    }

    return 0;
}

static int app_netif_wifi_driver_init(const app_netif_wifi_config_t *app_cfg) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    if (app_cfg->ap_enabled) {
        wifi_config_t wifi_ap_config = {
            .ap =
                {
#if APP_NETIF_WIFI_AP_SUPPORT_WPA3
                    .authmode    = WIFI_AUTH_WPA3_PSK,
                    .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else
                    .authmode = WIFI_AUTH_WPA2_PSK,
#endif
                    .channel        = app_cfg->ap_config.chan,
                    .max_connection = APP_NETIF_WIFI_AP_MAX_CLIENTS,
                    .pmf_cfg        = {.capable = true, .required = true},
                },
        };

        strncpy((char *)wifi_ap_config.ap.ssid, app_cfg->ap_config.ssid, sizeof(wifi_ap_config.ap.ssid));
        strncpy((char *)wifi_ap_config.ap.password, app_cfg->ap_config.pass, sizeof(wifi_ap_config.ap.password));

        esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
        if (err == ESP_ERR_INVALID_ARG) {
            return -1;
        }

        ESP_ERROR_CHECK(err);

        ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));
    }

    if (app_cfg->sta_enabled) {
        wifi_config_t wifi_sta_config = {
            .sta =
                {
                    .scan_method        = WIFI_ALL_CHANNEL_SCAN,
                    .failure_retry_cnt  = APP_NETIF_WIFI_STA_MAX_RETRIES,
                    .threshold.authmode = APP_NETIF_WIFI_STA_AUTH_THRESH,
                    .sae_pwe_h2e        = WPA3_SAE_PWE_BOTH,
                },
        };

        strncpy((char *)wifi_sta_config.sta.ssid, app_cfg->sta_config.ssid, sizeof(wifi_sta_config.sta.ssid));
        strncpy((char *)wifi_sta_config.sta.password, app_cfg->sta_config.pass, sizeof(wifi_sta_config.sta.password));

        esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
        if (err == ESP_ERR_INVALID_ARG) {
            return -1;
        }

        ESP_ERROR_CHECK(err);
    }

    if (app_cfg->ap_enabled || app_cfg->sta_enabled) {
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    return 0;
}

static int app_netif_wifi_driver_deinit(void) {
    ESP_ERROR_CHECK(esp_wifi_stop());
    return 0;
}

static void app_netif_wifi_manager_task(void *arguments) {
    app_netif_wifi_queue_item_t evt;

    for (;;) {
        /* -- */
        if (xQueueReceive(s_netif_wifi_mgr_queue, &evt, portMAX_DELAY) != pdPASS) {
            continue;
        }

        switch (evt.type) {
            case APP_NETIF_WIFI_MGR_CFG_RELOAD: {
                vTaskDelay(pdMS_TO_TICKS(500));

                app_netif_wifi_config_t cfg;

                app_netif_wifi_config_get(&cfg);
                app_netif_wifi_driver_deinit();
                app_netif_wifi_driver_init(&cfg);

                break;
            }

            case APP_NETIF_WIFI_MGR_STA_RECONNECT: {
                vTaskDelay(pdMS_TO_TICKS(1000));

                esp_wifi_connect();

                break;
            }
            default:
                break;
        }
    }
}

static void app_netif_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            app_netif_wifi_sta_reconnect();
            break;

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
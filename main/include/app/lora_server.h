#ifndef APP_LORA_SERVER_H
#define APP_LORA_SERVER_H

typedef enum {
    APP_LORA_SERVER_BW_125 = 0, /* 125kHz Bandwidth */
    APP_LORA_SERVER_BW_250,     /* 250kHz Bandwidth */
    APP_LORA_SERVER_BW_500,     /* 500kHz Bandwidth */
    APP_LORA_SERVER_BW_INVALID,
} app_lora_server_bw_t;

typedef enum {
    APP_LORA_SERVER_SF_5 = 0, /* Spreading Factor 5 */
    APP_LORA_SERVER_SF_6,     /* Spreading Factor 6 */
    APP_LORA_SERVER_SF_7,     /* Spreading Factor 7 */
    APP_LORA_SERVER_SF_8,     /* Spreading Factor 8 */
    APP_LORA_SERVER_SF_9,     /* Spreading Factor 9 */
    APP_LORA_SERVER_SF_10,    /* Spreading Factor 10 */
    APP_LORA_SERVER_SF_11,    /* Spreading Factor 11 */
    APP_LORA_SERVER_SF_12,    /* Spreading Factor 12 */
    APP_LORA_SERVER_SF_INVALID,
} app_lora_server_sf_t;

typedef enum {
    APP_LORA_SERVER_CR_1 = 0, /* Coding Rate 4/5 */
    APP_LORA_SERVER_CR_2,     /* Coding Rate 4/6 */
    APP_LORA_SERVER_CR_3,     /* Coding Rate 4/7 */
    APP_LORA_SERVER_CR_4,     /* Coding Rate 4/8 */
    APP_LORA_SERVER_CR_INVALID,
} app_lora_server_cr_t;

typedef struct {
    uint32_t             frequency;
    app_lora_server_bw_t bandwidth;
    app_lora_server_sf_t spreading_factor;
    app_lora_server_cr_t coding_rate;
    bool                 ldr_optimization;
} app_lora_server_config_t;

int  app_lora_server_init(void);
void app_lora_server_config_init(app_lora_server_config_t *config);
int  app_lora_server_config_set(const app_lora_server_config_t *config);
int  app_lora_server_config_get(app_lora_server_config_t *config);

#endif  // APP_LORA_SERVER_H

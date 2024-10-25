#ifndef APP_LORA_SERVER_H
#define APP_LORA_SERVER_H

#include "lora_modem.h"

typedef struct {
    bool                enabled;
    lora_modem_config_t modem_config;
} app_lora_server_config_t;

int  app_lora_server_init(void);
void app_lora_server_config_init(app_lora_server_config_t *config);
int  app_lora_server_config_set(const app_lora_server_config_t *config);
int  app_lora_server_config_get(app_lora_server_config_t *config);
int  app_lora_server_broadcast(const uint8_t *data, size_t length);

#endif  // APP_LORA_SERVER_H

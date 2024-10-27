/* IDF */
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "linenoise/linenoise.h"
#include "nvs.h"
#include "nvs_flash.h"

/* App */
#include "app/console/cmd_lora.h"
#include "app/lora_server.h"

static int app_console_lora_func(int argc, char **argv) {
    uint8_t *buf = malloc(1024);

    for (size_t i = 0; i < 1024; i++) {
        buf[i] = i;
    }

    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    app_lora_server_broadcast(buf, 1024);

    free(buf);
    return 0;
}

const esp_console_cmd_t app_console_cmd_lora = {
    .command = "lora",
    .help    = "LoRa control command",
    .hint    = NULL,
    .func    = app_console_lora_func,
};
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
    const uint8_t buf[4] = {0x00, 0x01, 0x02, 0x03};
    app_lora_server_broadcast(buf, 4);
    return 0;
}

const esp_console_cmd_t app_console_cmd_lora = {
    .command = "lora",
    .help    = "LoRa control command",
    .hint    = NULL,
    .func    = app_console_lora_func,
};
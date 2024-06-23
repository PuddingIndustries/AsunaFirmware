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
#include "app/console/cmd_free.h"

static int app_console_free_func(int argc, char **argv) {
    printf("Free Heap size: %" PRIu32 "\n", esp_get_free_heap_size());

    if (argc > 1) {
        uint32_t heap_wm = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
        printf("Minimum RTOS heap size watermark: %" PRIu32 "\n", heap_wm);
    }

    return 0;
}

const esp_console_cmd_t app_console_cmd_free = {
    .command = "free",
    .help    = "Get the current size of free heap memory",
    .hint    = NULL,
    .func    = &app_console_free_func,
};

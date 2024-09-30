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

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* App */
#include "app/console/cmd_ps.h"

#define APP_CONSOLE_PS_MAX_TASKS_NUM 64

static const char *LOG_TAG = "asuna_console_ps";

static const char *s_task_state_string[] = {
    [eRunning] = "RUNNING",     [eReady] = "READY",     [eBlocked] = "BLOCKED",
    [eSuspended] = "SUSPENDED", [eDeleted] = "DELETED", [eInvalid] = "INVALID",
};

int app_console_ps_status_compare(const void *a, const void *b) {
    const TaskStatus_t *ap = (TaskStatus_t *)a;
    const TaskStatus_t *bp = (TaskStatus_t *)b;

    return  (int)bp->uxBasePriority - (int)ap->uxBasePriority;
}

static int app_console_ps_func(int argc, char **argv) {
    int ret = 0;

    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();

    if (num_tasks > APP_CONSOLE_PS_MAX_TASKS_NUM) {
        num_tasks = APP_CONSOLE_PS_MAX_TASKS_NUM;
    }

    TaskStatus_t *status_list = malloc(sizeof(TaskStatus_t) * num_tasks);
    if (status_list == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to allocate task status list.");

        return -1;
    }

    uint32_t run_time;

    if (uxTaskGetSystemState(status_list, num_tasks, &run_time) != num_tasks) {
        ESP_LOGE(LOG_TAG, "Failed to retrieve task list.");
        ret = -1;
        goto free_list_exit;
    }

    qsort(status_list, num_tasks, sizeof(TaskStatus_t), app_console_ps_status_compare);

    printf("Task Status:\n");
    printf("%11s\t", "xTaskNumber");
    printf("%10s\t", "xHandle");
    printf("%16s\t", "pcTaskName");
    printf("%10s\t", "stackHigh");
    printf("%10s\t", "basePri");
    printf("%10s\t", "currPri");
    printf("%10s\t", "eState");
    printf("%10s\t", "runTime");
    printf("\n");

    for (uint8_t i = 0; i < 128; i++) {
        printf("-");
    }

    printf("\n");

    for (size_t i = 0; i < num_tasks; i++) {
        printf("%11d\t", status_list[i].xTaskNumber);
        printf("%10p\t", status_list[i].xHandle);
        printf("%16s\t", status_list[i].pcTaskName);
        printf("%10ld\t", status_list[i].usStackHighWaterMark);
        printf("%10d\t", status_list[i].uxBasePriority);
        printf("%10d\t", status_list[i].uxCurrentPriority);
        printf("%10s\t", s_task_state_string[status_list[i].eCurrentState]);
        printf("%10lu\t", status_list[i].ulRunTimeCounter);

        printf("\n");
    }

free_list_exit:
    free(status_list);
    return ret;
}

const esp_console_cmd_t app_console_cmd_ps = {
    .command = "ps",
    .help    = "Get RTOS task information",
    .hint    = NULL,
    .func    = app_console_ps_func,
};
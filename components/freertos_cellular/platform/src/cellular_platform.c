#include "cellular_platform.h"

typedef struct {
    void  (*thread_code)(void*);
    void* argument;
} Platform_ThreadDef_t;

static void Platform_ThreadWrapper(void* arguments);

bool PlatformMutex_Create(PlatformMutex_t* pNewMutex, bool recursive) {
    pNewMutex->bRecursive = recursive;

    if (recursive) {
        pNewMutex->xMutex = xSemaphoreCreateRecursiveMutex();
    } else {
        pNewMutex->xMutex = xSemaphoreCreateMutex();
    }

    if (pNewMutex->xMutex == NULL) {
        return false;
    }

    return true;
}

void PlatformMutex_Destroy(PlatformMutex_t* pMutex) {
    vSemaphoreDelete(pMutex->xMutex);
}

void PlatformMutex_Lock(PlatformMutex_t* pMutex) {
    if (pMutex->bRecursive) {
        xSemaphoreTakeRecursive(pMutex->xMutex, portMAX_DELAY);
    } else {
        xSemaphoreTake(pMutex->xMutex, portMAX_DELAY);
    }
}

bool PlatformMutex_TryLock(PlatformMutex_t* pMutex) {
    BaseType_t ret;

    if (pMutex->bRecursive) {
        ret = xSemaphoreTakeRecursive(pMutex->xMutex, 0);
    } else {
        ret = xSemaphoreTake(pMutex->xMutex, 0);
    }

    if (ret != pdPASS) {
        return false;
    }

    return true;
}

void PlatformMutex_Unlock(PlatformMutex_t* pMutex) {
    if (pMutex->bRecursive) {
        xSemaphoreGiveRecursive(pMutex->xMutex);
    } else {
        xSemaphoreGive(pMutex->xMutex);
    }
}

bool Platform_CreateDetachedThread(void (*threadRoutine)(void*), void* pArgument, int32_t priority, size_t stackSize) {
    /* Note: this will be free'd in Platform_ThreadWrapper() */
    Platform_ThreadDef_t* thread_def = Platform_Malloc(sizeof(Platform_ThreadDef_t));
    if (thread_def == NULL) {
        return false;
    }

    thread_def->thread_code = threadRoutine;
    thread_def->argument    = pArgument;

    BaseType_t ret = xTaskCreate(Platform_ThreadWrapper, "A_CELL", stackSize, thread_def, priority, NULL);
    if (ret != pdPASS) {
        Platform_Free(thread_def);
        return false;
    }

    return true;
}

static void Platform_ThreadWrapper(void* arguments) {
    Platform_ThreadDef_t* thread_def = arguments;

    /* Run the task code. */
    thread_def->thread_code(thread_def->argument);

    Platform_Free(thread_def);
    vTaskDelete(NULL);
}
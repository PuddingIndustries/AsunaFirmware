#include "cellular_platform.h"

bool PlatformMutex_Create(PlatformMutex_t* pNewMutex, bool recursive) {
    pNewMutex->bRecursive = recursive;

    if (recursive) {
        pNewMutex->xMutex = xSemaphoreCreateRecursiveMutex();
    } else {
        pNewMutex->xMutex = xSemaphoreCreateBinary();
    }

    if (pNewMutex->xMutex == NULL) {
        return false;
    }

    PlatformMutex_Unlock(pNewMutex);

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
    BaseType_t ret = xTaskCreate(threadRoutine, "CEL", stackSize, pArgument, priority, NULL);
    if (ret != pdPASS) {
        return false;
    }

    return true;
}

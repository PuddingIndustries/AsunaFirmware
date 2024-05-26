#ifndef APP_CELLUAR_PLATFORM_H
#define APP_CELLUAR_PLATFORM_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define Platform_Malloc pvPortMalloc
#define Platform_Free   vPortFree

#define PlatformEventGroupHandle_t        EventGroupHandle_t
#define PlatformEventGroup_Delete         vEventGroupDelete
#define PlatformEventGroup_ClearBits      xEventGroupClearBits
#define PlatformEventGroup_Create         xEventGroupCreate
#define PlatformEventGroup_GetBits        xEventGroupGetBits
#define PlatformEventGroup_SetBits        xEventGroupSetBits
#define PlatformEventGroup_SetBitsFromISR xEventGroupSetBitsFromISR
#define PlatformEventGroup_WaitBits       xEventGroupWaitBits
#define PlatformEventGroup_EventBits      EventBits_t

#define PlatformTickType        TickType_t
#define Platform_Delay(delayMs) vTaskDelay(pdMS_TO_TICKS(delayMs))

#define PlatformSpinLockType     portMUX_TYPE
#define PlatformSpinLockInit(x)  portMUX_INITIALIZE(x)
#define PlatformEnterCritical(x) taskENTER_CRITICAL(x)
#define PlatformExitCritical(x)  taskEXIT_CRITICAL(x)

#define PLATFORM_THREAD_DEFAULT_STACK_SIZE (2048U)
#define PLATFORM_THREAD_DEFAULT_PRIORITY   (tskIDLE_PRIORITY + 2U)

typedef struct PlatformMutex {
    SemaphoreHandle_t xMutex;
    bool              bRecursive;
} PlatformMutex_t;

bool PlatformMutex_Create(PlatformMutex_t* pNewMutex, bool recursive);
void PlatformMutex_Destroy(PlatformMutex_t* pMutex);
void PlatformMutex_Lock(PlatformMutex_t* pMutex);
bool PlatformMutex_TryLock(PlatformMutex_t* pMutex);
void PlatformMutex_Unlock(PlatformMutex_t* pMutex);

bool Platform_CreateDetachedThread(void (*threadRoutine)(void*), void* pArgument, int32_t priority, size_t stackSize);

#endif  // APP_CELLUAR_PLATFORM_H

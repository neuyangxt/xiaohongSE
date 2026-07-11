#ifndef OHOS_LITEOS_MEDIA_TASK_H
#define OHOS_LITEOS_MEDIA_TASK_H

#include <stdint.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "los_task.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#ifndef LOS_WAIT_FOREVER
#define LOS_WAIT_FOREVER 0xffffffffU
#endif

#ifndef LOS_ERRNO_TSK_NO_MEMORY
#define LOS_ERRNO_TSK_NO_MEMORY 0x03000200U
#endif

#ifndef LOSCFG_STACK_POINT_ALIGN_SIZE
#define LOSCFG_STACK_POINT_ALIGN_SIZE 16U
#endif

extern UINT32 LOS_MS2Tick(UINT32 millisec);
extern UINT32 LOS_IntLock(VOID);
extern VOID LOS_IntRestore(UINT32 intSave);

typedef void (*OhosLiteosVoidTaskEntry)(void *arg);

typedef struct {
    OhosLiteosVoidTaskEntry entry;
    void *arg;
    void *externalStack;
} OhosLiteosTaskCtx;

static inline UINT32 OhosLiteosMsToTicks(UINT32 ms)
{
    UINT32 ticks = LOS_MS2Tick(ms);
    if (ticks == 0U && ms != 0U) {
        ticks = 1U;
    }
    return ticks;
}

static inline void OhosLiteosDelayMs(UINT32 ms)
{
    UINT32 ticks = OhosLiteosMsToTicks(ms);
    if (ticks != 0U) {
        (void)LOS_TaskDelay(ticks);
    }
}

static VOID *OhosLiteosTaskTrampoline(UINT32 arg)
{
    OhosLiteosTaskCtx *ctx = (OhosLiteosTaskCtx *)(uintptr_t)arg;
    OhosLiteosVoidTaskEntry entry = NULL;
    void *entryArg = NULL;

    if (ctx != NULL) {
        entry = ctx->entry;
        entryArg = ctx->arg;
        free(ctx);
    }

    if (entry != NULL) {
        entry(entryArg);
    }

    return NULL;
}

static inline UINT32 OhosLiteosCreateTask(const char *name,
                                          OhosLiteosVoidTaskEntry entry,
                                          void *arg,
                                          UINT16 prio,
                                          UINT32 stackSize,
                                          UINT32 *taskId)
{
    if (entry == NULL) {
        return 1U;
    }

    OhosLiteosTaskCtx *ctx = (OhosLiteosTaskCtx *)malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return 2U;
    }

    ctx->entry = entry;
    ctx->arg = arg;
    ctx->externalStack = NULL;

    UINT32 localTaskId = 0;
    TSK_INIT_PARAM_S initParam = {0};
    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OhosLiteosTaskTrampoline;
    initParam.usTaskPrio = prio;
    initParam.uwStackSize = stackSize;
    initParam.pcName = (CHAR *)(name != NULL ? name : "ohos_media");
    initParam.uwArg = (UINT32)(uintptr_t)ctx;
    initParam.uwResved = 0;

    UINT32 ret = LOS_TaskCreate(&localTaskId, &initParam);
    if (ret == LOS_ERRNO_TSK_NO_MEMORY) {
        void *stackMem = heap_caps_aligned_alloc(LOSCFG_STACK_POINT_ALIGN_SIZE,
                                                 stackSize,
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (stackMem != NULL) {
            ctx->externalStack = stackMem;
            localTaskId = 0;
            initParam.stackAddr = (UINTPTR)stackMem;
            ret = LOS_TaskCreate(&localTaskId, &initParam);
            esp_rom_printf("[OHOS-MEDIA-TASK] create %s fallback external stack=%p size=0x%x ret=%u taskId=%u\n",
                           name != NULL ? name : "ohos_media",
                           stackMem,
                           stackSize,
                           ret,
                           localTaskId);
        } else {
            esp_rom_printf("[OHOS-MEDIA-TASK] create %s external stack alloc failed size=0x%x\n",
                           name != NULL ? name : "ohos_media",
                           stackSize);
        }
    }

    if (ret != LOS_OK) {
        if (ctx->externalStack != NULL) {
            heap_caps_free(ctx->externalStack);
        }
        free(ctx);
        return ret;
    }

    if (taskId != NULL) {
        *taskId = localTaskId;
    }

    return LOS_OK;
}

#endif

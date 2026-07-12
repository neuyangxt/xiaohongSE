#include "ohos_service_common.h"

#include <stddef.h>

#include "ohos_errno.h"
#include "esp_rom_sys.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

uint32_t OhosServiceWaitReady(volatile const uint32_t *initCount,
                              const Identity *identity,
                              uint32_t timeoutTicks)
{
    if (initCount == NULL || identity == NULL) {
        return 0U;
    }

    for (uint32_t i = 0; i < timeoutTicks; ++i) {
        if ((*initCount > 0U) && (identity->queueId != NULL)) {
            return 1U;
        }
        LOS_TaskDelay(1);
    }

    return ((*initCount > 0U) && (identity->queueId != NULL)) ? 1U : 0U;
}

int32 OhosServiceSendSimpleRequest(const Identity *identity,
                                   int16 msgId,
                                   uint32_t msgValue)
{
    if (identity == NULL || identity->queueId == NULL) {
        return EC_FAILURE;
    }

    Request req = {
        .msgId = msgId,
        .len = 0,
        .data = NULL,
        .msgValue = msgValue,
    };

    return SAMGR_SendRequest(identity, &req, NULL);
}

uint32_t OhosServiceCreateTask(const char *tag,
                               const char *name,
                               TSK_ENTRY_FUNC entry,
                               UINT32 prio,
                               UINT32 stackSize,
                               UINT32 arg,
                               UINT32 *taskId)
{
    if (entry == NULL || taskId == NULL) {
        return 1U;
    }

    TSK_INIT_PARAM_S initParam = {0};

    initParam.pfnTaskEntry = entry;
    initParam.usTaskPrio = prio;
    initParam.uwStackSize = stackSize;
    initParam.pcName = (char *)name;
    initParam.uwArg = arg;
    initParam.uwResved = 0;

    UINT32 ret = LOS_TaskCreate(taskId, &initParam);

    esp_rom_printf("%s common task create name=%s ret=%u taskId=%u prio=%u stack=0x%x\n",
                   tag ? tag : "[OHOS-SVC]",
                   name ? name : "null",
                   ret,
                   *taskId,
                   prio,
                   stackSize);

    return ret;
}

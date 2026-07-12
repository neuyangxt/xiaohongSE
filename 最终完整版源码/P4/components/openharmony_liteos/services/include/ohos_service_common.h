#ifndef OHOS_SERVICE_COMMON_H
#define OHOS_SERVICE_COMMON_H

#include <stdint.h>

#include "ohos_types.h"
#include "los_task.h"
#include "samgr_lite.h"
#include "service_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t OhosServiceWaitReady(volatile const uint32_t *initCount,
                              const Identity *identity,
                              uint32_t timeoutTicks);

int32 OhosServiceSendSimpleRequest(const Identity *identity,
                                   int16 msgId,
                                   uint32_t msgValue);

uint32_t OhosServiceCreateTask(const char *tag,
                               const char *name,
                               TSK_ENTRY_FUNC entry,
                               UINT32 prio,
                               UINT32 stackSize,
                               UINT32 arg,
                               UINT32 *taskId);

#ifdef __cplusplus
}
#endif

#endif

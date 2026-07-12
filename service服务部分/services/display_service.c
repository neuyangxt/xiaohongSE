#include <stdint.h>

#include "ohos_types.h"
#include "ohos_errno.h"
#include "los_task.h"
#include "samgr_lite.h"
#include "service_impl.h"
#include "ohos_service_msg.h"
#include "ohos_service_common.h"
#include "ohos_liteos_media_task.h"
#include "esp_rom_sys.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#define OHOS_DISPLAY_SERVICE_NAME "DisplayService"

#define OHOS_DISPLAY_VERIFY_TASK_PRIO   25
#define OHOS_DISPLAY_VERIFY_TASK_STACK  0x1000
#define OHOS_DISPLAY_HW_TASK_PRIO       24
#define OHOS_DISPLAY_HW_TASK_STACK      0x7000

static volatile uint32_t g_display_service_init_count = 0;
static volatile uint32_t g_display_service_msg_count = 0;
static volatile uint32_t g_display_service_queue_ok = 0;
static volatile uint32_t g_display_service_send_ok = 0;
static volatile uint32_t g_display_service_status_ok = 0;
static volatile uint32_t g_display_service_selftest_ok = 0;
static volatile uint32_t g_display_service_draw_ok = 0;
static UINT32 g_display_hw_task_id = 0;

static Identity g_display_service_identity = {
    .serviceId = -1,
    .featureId = -1,
    .queueId = NULL,
};

static const char *DisplayServiceGetName(Service *service)
{
    (void)service;
    return OHOS_DISPLAY_SERVICE_NAME;
}

static BOOL DisplayServiceInitialize(Service *service, Identity identity)
{
    (void)service;

    g_display_service_init_count++;
    g_display_service_identity = identity;
    g_display_service_queue_ok = (identity.queueId != NULL);

    /*
     * S43A only validates DisplayService framework.
     * Real LCD/MIPI/SPI panel driver will be connected in later stages.
     */
    esp_rom_printf("[OHOS-S46A] DisplayService init count=%u serviceId=%d featureId=%d queue=%p queueOk=%u\n",
                   g_display_service_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId,
                   g_display_service_queue_ok);

    return TRUE;
}

static BOOL DisplayServiceMessageHandle(Service *service, Request *request)
{
    (void)service;

    g_display_service_msg_count++;

    BOOL reqOk = FALSE;
    int msgId = request ? request->msgId : -1;
    uint32_t cmd = request ? request->msgValue : 0xffffffffU;

    if (request != NULL) {
        if (request->msgId == OHOS_SERVICE_MSG_DISPLAY_GET_STATUS &&
            request->msgValue == OHOS_SERVICE_CMD_GET_STATUS) {
            g_display_service_status_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_DISPLAY_SELF_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_SELF_TEST) {
            g_display_service_selftest_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_DISPLAY_DRAW_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_START) {
            /*
             * S43A draw test is a logical interface test.
             * It does not access real LCD hardware yet.
             */
            g_display_service_draw_ok = TRUE;
            reqOk = TRUE;
        }
    }

    esp_rom_printf("[OHOS-S46A] DisplayService message count=%u req=%p msgId=%d cmd=%u reqOk=%u statusOk=%u selfTestOk=%u drawOk=%u\n",
                   g_display_service_msg_count,
                   request,
                   msgId,
                   cmd,
                   reqOk,
                   g_display_service_status_ok,
                   g_display_service_selftest_ok,
                   g_display_service_draw_ok);

    return TRUE;
}

static TaskConfig DisplayServiceGetTaskConfig(Service *service)
{
    (void)service;

    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = PRI_NORMAL,
        .stackSize = 0x1000,
        .queueSize = 8,
        .taskFlags = SINGLE_TASK,
    };

    return config;
}

static Service g_display_service = {
    .GetName = DisplayServiceGetName,
    .Initialize = DisplayServiceInitialize,
    .MessageHandle = DisplayServiceMessageHandle,
    .GetTaskConfig = DisplayServiceGetTaskConfig,
};

static int32 DisplayServiceSendRequest(int16 msgId, uint32_t cmd)
{
    return OhosServiceSendSimpleRequest(&g_display_service_identity, msgId, cmd);
}

static VOID *OhosDisplayServiceVerifyTask(UINT32 arg)
{
    (void)arg;

    BOOL ready = OhosServiceWaitReady(&g_display_service_init_count,
                                      &g_display_service_identity,
                                      200);

    BOOL initOk = (g_display_service_init_count > 0);
    BOOL queueOk = (g_display_service_identity.queueId != NULL);

    int32 statusRet = EC_FAILURE;
    int32 selfTestRet = EC_FAILURE;
    int32 drawRet = EC_FAILURE;

    if (ready) {
        statusRet = DisplayServiceSendRequest(OHOS_SERVICE_MSG_DISPLAY_GET_STATUS,
                                              OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        selfTestRet = DisplayServiceSendRequest(OHOS_SERVICE_MSG_DISPLAY_SELF_TEST,
                                                OHOS_SERVICE_CMD_SELF_TEST);
        LOS_TaskDelay(10);

        drawRet = DisplayServiceSendRequest(OHOS_SERVICE_MSG_DISPLAY_DRAW_TEST,
                                            OHOS_SERVICE_CMD_START);
    }

    g_display_service_send_ok = (statusRet == EC_SUCCESS &&
                                 selfTestRet == EC_SUCCESS &&
                                 drawRet == EC_SUCCESS);

    esp_rom_printf("[OHOS-S46A] DisplayService send status=%d selfTest=%d draw=%d sendOk=%u initOk=%u queueOk=%u ready=%u\n",
                   statusRet,
                   selfTestRet,
                   drawRet,
                   g_display_service_send_ok,
                   initOk,
                   queueOk,
                   ready);

    for (int i = 0; i < 200; ++i) {
        if (g_display_service_msg_count >= 3 &&
            g_display_service_status_ok &&
            g_display_service_selftest_ok &&
            g_display_service_draw_ok) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL msgOk = (g_display_service_msg_count >= 3 &&
                  g_display_service_status_ok &&
                  g_display_service_selftest_ok &&
                  g_display_service_draw_ok);

    BOOL ok = (initOk &&
               queueOk &&
               ready &&
               g_display_service_send_ok &&
               msgOk);

    esp_rom_printf("[OHOS-S46A] DisplayService verify init=%u queue=%u ready=%u send=%u msg=%u msgCnt=%u status=%u selfTest=%u draw=%u ok=%u\n",
                   initOk,
                   queueOk,
                   ready,
                   g_display_service_send_ok,
                   msgOk,
                   g_display_service_msg_count,
                   g_display_service_status_ok,
                   g_display_service_selftest_ok,
                   g_display_service_draw_ok,
                   ok);

    return NULL;
}


uint32_t OhosDisplayServiceSelfTest(void)
{
    uint32_t ready = OhosServiceWaitReady(&g_display_service_init_count,
                                          &g_display_service_identity,
                                          200);
    uint32_t baseMsgCnt = g_display_service_msg_count;

    int32 ret0 = EC_FAILURE;
    int32 ret1 = EC_FAILURE;
    int32 ret2 = EC_FAILURE;

    if (ready) {
        ret0 = DisplayServiceSendRequest(OHOS_SERVICE_MSG_DISPLAY_GET_STATUS,
                                 OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        ret1 = DisplayServiceSendRequest(OHOS_SERVICE_MSG_DISPLAY_SELF_TEST,
                                 OHOS_SERVICE_CMD_SELF_TEST);
        LOS_TaskDelay(10);

        ret2 = DisplayServiceSendRequest(OHOS_SERVICE_MSG_DISPLAY_DRAW_TEST,
                                 OHOS_SERVICE_CMD_START);
        LOS_TaskDelay(10);
    }

    for (int i = 0; i < 200; ++i) {
        if (g_display_service_msg_count >= baseMsgCnt + 3) {
            break;
        }
        LOS_TaskDelay(1);
    }

    uint32_t msgOk = (g_display_service_msg_count >= baseMsgCnt + 3);
    uint32_t sendOk = (ret0 == EC_SUCCESS &&
                       ret1 == EC_SUCCESS &&
                       ret2 == EC_SUCCESS);
    uint32_t ok = (ready && sendOk && msgOk);

    esp_rom_printf("[OHOS-S46A] DisplayService selftest ready=%u send=%u msg=%u base=%u now=%u ok=%u\n",
                   ready,
                   sendOk,
                   msgOk,
                   baseMsgCnt,
                   g_display_service_msg_count,
                   ok);

    return ok ? LOS_OK : 1U;
}


extern void OhosMainlineDisplayStartRealHw(void);
uint32_t OhosDisplayServiceStartRealHw(void);

static volatile uint32_t g_display_real_hw_started = 0;

static void OhosDisplayServiceRealHwTask(void *arg)
{
    (void)arg;

    esp_rom_printf("[OHOS-DISPLAY-HW] DisplayService LiteOS real HW task enter\n");
    OhosDisplayServiceStartRealHw();
    esp_rom_printf("[OHOS-DISPLAY-HW] DisplayService LiteOS real HW task exit started=%u\n",
                   g_display_real_hw_started);
}

uint32_t OhosDisplayServiceStartRealHw(void)
{
    if (g_display_real_hw_started) {
        esp_rom_printf("[OHOS-DISPLAY-HW] DisplayService real HW already started\n");
        return 0;
    }

    esp_rom_printf("[OHOS-DISPLAY-HW] DisplayService start real MIPI DSI/LVGL display path\n");
    OhosMainlineDisplayStartRealHw();
    g_display_real_hw_started = 1;

    esp_rom_printf("[OHOS-DISPLAY-HW] DisplayService real HW start done\n");
    return 0;
}

uint32_t OhosDisplayServiceRegister(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    if (samgr == NULL) {
        esp_rom_printf("[OHOS-S46A] DisplayService register failed: samgr null\n");
        return 1U;
    }

    BOOL regOk = samgr->RegisterService(&g_display_service);

    esp_rom_printf("[OHOS-S46A] DisplayService register regOk=%u\n", regOk);

    return (regOk == TRUE) ? LOS_OK : 2U;
}

uint32_t OhosDisplayServiceStartTasks(void)
{
    if (g_display_real_hw_started || g_display_hw_task_id != 0U) {
        esp_rom_printf("[OHOS-DISPLAY-HW] DisplayService real HW task already requested taskId=%u started=%u\n",
                       g_display_hw_task_id,
                       g_display_real_hw_started);
        return LOS_OK;
    }

    UINT32 ret = OhosLiteosCreateTask("ohos_display_hw",
                                      OhosDisplayServiceRealHwTask,
                                      NULL,
                                      OHOS_DISPLAY_HW_TASK_PRIO,
                                      OHOS_DISPLAY_HW_TASK_STACK,
                                      &g_display_hw_task_id);
    esp_rom_printf("[OHOS-DISPLAY-HW] DisplayService real HW task create ret=%u taskId=%u prio=%u stack=0x%x\n",
                   ret,
                   g_display_hw_task_id,
                   OHOS_DISPLAY_HW_TASK_PRIO,
                   OHOS_DISPLAY_HW_TASK_STACK);
    return ret;
}

uint32_t OhosDisplayServiceStart(void)
{
    uint32_t ret = OhosDisplayServiceRegister();

    if (ret != LOS_OK) {
        return ret;
    }

    SAMGR_Bootstrap();

    ret = OhosDisplayServiceStartTasks();

    return ret;
}

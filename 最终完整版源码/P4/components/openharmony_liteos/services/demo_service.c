#include <stdint.h>

#include "ohos_types.h"
#include "ohos_errno.h"
#include "los_task.h"
#include "samgr_lite.h"
#include "service_impl.h"
#include "esp_rom_sys.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#define OHOS_DEMO_SERVICE_NAME "DemoService"
#define OHOS_DEMO_MSG_ID       3601
#define OHOS_DEMO_MSG_VALUE    0x36360036U

#define OHOS_DEMO_VERIFY_TASK_PRIO   25
#define OHOS_DEMO_VERIFY_TASK_STACK  0x1000

static volatile uint32_t g_demo_service_init_count = 0;
static volatile uint32_t g_demo_service_msg_count = 0;
static volatile uint32_t g_demo_service_queue_ok = 0;
static volatile uint32_t g_demo_service_msg_ok = 0;
static volatile uint32_t g_demo_service_send_ok = 0;

static Identity g_demo_service_identity = {
    .serviceId = -1,
    .featureId = -1,
    .queueId = NULL,
};

static const char *DemoServiceGetName(Service *service)
{
    (void)service;
    return OHOS_DEMO_SERVICE_NAME;
}

static BOOL DemoServiceInitialize(Service *service, Identity identity)
{
    (void)service;

    g_demo_service_init_count++;
    g_demo_service_identity = identity;
    g_demo_service_queue_ok = (identity.queueId != NULL);

    esp_rom_printf("[OHOS-S36C] DemoService init count=%u serviceId=%d featureId=%d queue=%p queueOk=%u\n",
                   g_demo_service_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId,
                   g_demo_service_queue_ok);

    return TRUE;
}

static BOOL DemoServiceMessageHandle(Service *service, Request *request)
{
    (void)service;

    g_demo_service_msg_count++;

    BOOL reqOk = (request != NULL &&
                  request->msgId == OHOS_DEMO_MSG_ID &&
                  request->msgValue == OHOS_DEMO_MSG_VALUE);

    if (reqOk) {
        g_demo_service_msg_ok = TRUE;
    }

    esp_rom_printf("[OHOS-S36C] DemoService message count=%u req=%p msgId=%d msgValue=0x%x reqOk=%u\n",
                   g_demo_service_msg_count,
                   request,
                   request ? request->msgId : -1,
                   request ? request->msgValue : 0,
                   reqOk);

    return TRUE;
}

static TaskConfig DemoServiceGetTaskConfig(Service *service)
{
    (void)service;

    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = PRI_NORMAL,
        .stackSize = 0x1000,
        .queueSize = 4,
        .taskFlags = SINGLE_TASK,
    };

    return config;
}

static Service g_demo_service = {
    .GetName = DemoServiceGetName,
    .Initialize = DemoServiceInitialize,
    .MessageHandle = DemoServiceMessageHandle,
    .GetTaskConfig = DemoServiceGetTaskConfig,
};

static VOID *OhosDemoServiceVerifyTask(UINT32 arg)
{
    (void)arg;

    for (int i = 0; i < 200; ++i) {
        if (g_demo_service_init_count > 0 && g_demo_service_identity.queueId != NULL) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL initOk = (g_demo_service_init_count > 0);
    BOOL queueOk = (g_demo_service_identity.queueId != NULL);

    int32 sendRet = EC_FAILURE;

    if (initOk && queueOk) {
        Request req = {
            .msgId = OHOS_DEMO_MSG_ID,
            .len = 0,
            .data = NULL,
            .msgValue = OHOS_DEMO_MSG_VALUE,
        };

        sendRet = SAMGR_SendRequest(&g_demo_service_identity, &req, NULL);
        g_demo_service_send_ok = (sendRet == EC_SUCCESS);
    }

    esp_rom_printf("[OHOS-S36C] DemoService send sendRet=%d sendOk=%u initOk=%u queueOk=%u\n",
                   sendRet,
                   g_demo_service_send_ok,
                   initOk,
                   queueOk);

    for (int i = 0; i < 200; ++i) {
        if (g_demo_service_msg_ok == TRUE) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL ok = (initOk &&
               queueOk &&
               g_demo_service_send_ok &&
               g_demo_service_msg_ok &&
               g_demo_service_msg_count > 0);

    esp_rom_printf("[OHOS-S36C] DemoService message verify init=%u queue=%u send=%u msg=%u msgCnt=%u ok=%u\n",
                   initOk,
                   queueOk,
                   g_demo_service_send_ok,
                   g_demo_service_msg_ok,
                   g_demo_service_msg_count,
                   ok);

    return NULL;
}

uint32_t OhosDemoServiceStart(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    if (samgr == NULL) {
        esp_rom_printf("[OHOS-S36C] DemoService start failed: samgr null\n");
        return 1U;
    }

    BOOL regOk = samgr->RegisterService(&g_demo_service);

    esp_rom_printf("[OHOS-S36C] DemoService register regOk=%u\n", regOk);

    if (regOk != TRUE) {
        return 2U;
    }

    SAMGR_Bootstrap();

    TSK_INIT_PARAM_S initParam = {0};
    UINT32 verifyTaskId = 0;

    initParam.pfnTaskEntry = OhosDemoServiceVerifyTask;
    initParam.usTaskPrio = OHOS_DEMO_VERIFY_TASK_PRIO;
    initParam.uwStackSize = OHOS_DEMO_VERIFY_TASK_STACK;
    initParam.pcName = "ohos_demo_verify";
    initParam.uwArg = 0;
    initParam.uwResved = 0;

    UINT32 ret = LOS_TaskCreate(&verifyTaskId, &initParam);

    esp_rom_printf("[OHOS-S36C] DemoService verify task create ret=%u taskId=%u prio=%u\n",
                   ret,
                   verifyTaskId,
                   initParam.usTaskPrio);

    return ret;
}

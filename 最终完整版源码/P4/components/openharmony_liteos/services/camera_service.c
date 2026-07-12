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

#define OHOS_CAMERA_SERVICE_NAME "CameraService"

#define OHOS_CAMERA_VERIFY_TASK_PRIO   25
#define OHOS_CAMERA_VERIFY_TASK_STACK  0x1000
#define OHOS_CAMERA_PREVIEW_TASK_PRIO  25
#define OHOS_CAMERA_PREVIEW_TASK_STACK 0x3000

static volatile uint32_t g_camera_service_init_count = 0;
static volatile uint32_t g_camera_service_msg_count = 0;
static volatile uint32_t g_camera_service_queue_ok = 0;
static volatile uint32_t g_camera_service_send_ok = 0;
static volatile uint32_t g_camera_service_status_ok = 0;
static volatile uint32_t g_camera_service_selftest_ok = 0;
static volatile uint32_t g_camera_service_capture_ok = 0;
static volatile uint32_t g_camera_service_stream_ok = 0;

static Identity g_camera_service_identity = {
    .serviceId = -1,
    .featureId = -1,
    .queueId = NULL,
};

static const char *CameraServiceGetName(Service *service)
{
    (void)service;
    return OHOS_CAMERA_SERVICE_NAME;
}

static BOOL CameraServiceInitialize(Service *service, Identity identity)
{
    (void)service;

    g_camera_service_init_count++;
    g_camera_service_identity = identity;
    g_camera_service_queue_ok = (identity.queueId != NULL);

    /*
     * S45A only validates CameraService framework.
     * Real CSI / camera sensor driver will be connected in later stages.
     */
    esp_rom_printf("[OHOS-S46A] CameraService init count=%u serviceId=%d featureId=%d queue=%p queueOk=%u\n",
                   g_camera_service_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId,
                   g_camera_service_queue_ok);

    return TRUE;
}

static BOOL CameraServiceMessageHandle(Service *service, Request *request)
{
    (void)service;

    g_camera_service_msg_count++;

    BOOL reqOk = FALSE;
    int msgId = request ? request->msgId : -1;
    uint32_t cmd = request ? request->msgValue : 0xffffffffU;

    if (request != NULL) {
        if (request->msgId == OHOS_SERVICE_MSG_CAMERA_GET_STATUS &&
            request->msgValue == OHOS_SERVICE_CMD_GET_STATUS) {
            g_camera_service_status_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_CAMERA_SELF_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_SELF_TEST) {
            g_camera_service_selftest_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_CAMERA_CAPTURE_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_START) {
            g_camera_service_capture_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_CAMERA_STREAM_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_START) {
            g_camera_service_stream_ok = TRUE;
            reqOk = TRUE;
        }
    }

    esp_rom_printf("[OHOS-S46A] CameraService message count=%u req=%p msgId=%d cmd=%u reqOk=%u statusOk=%u selfTestOk=%u captureOk=%u streamOk=%u\n",
                   g_camera_service_msg_count,
                   request,
                   msgId,
                   cmd,
                   reqOk,
                   g_camera_service_status_ok,
                   g_camera_service_selftest_ok,
                   g_camera_service_capture_ok,
                   g_camera_service_stream_ok);

    return TRUE;
}

static TaskConfig CameraServiceGetTaskConfig(Service *service)
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

static Service g_camera_service = {
    .GetName = CameraServiceGetName,
    .Initialize = CameraServiceInitialize,
    .MessageHandle = CameraServiceMessageHandle,
    .GetTaskConfig = CameraServiceGetTaskConfig,
};

static int32 CameraServiceSendRequest(int16 msgId, uint32_t cmd)
{
    return OhosServiceSendSimpleRequest(&g_camera_service_identity, msgId, cmd);
}

static VOID *OhosCameraServiceVerifyTask(UINT32 arg)
{
    (void)arg;

    BOOL ready = OhosServiceWaitReady(&g_camera_service_init_count,
                                      &g_camera_service_identity,
                                      200);

    BOOL initOk = (g_camera_service_init_count > 0);
    BOOL queueOk = (g_camera_service_identity.queueId != NULL);

    int32 statusRet = EC_FAILURE;
    int32 selfTestRet = EC_FAILURE;
    int32 captureRet = EC_FAILURE;
    int32 streamRet = EC_FAILURE;

    if (ready) {
        statusRet = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_GET_STATUS,
                                             OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        selfTestRet = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_SELF_TEST,
                                               OHOS_SERVICE_CMD_SELF_TEST);
        LOS_TaskDelay(10);

        captureRet = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_CAPTURE_TEST,
                                              OHOS_SERVICE_CMD_START);
        LOS_TaskDelay(10);

        streamRet = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_STREAM_TEST,
                                             OHOS_SERVICE_CMD_START);
    }

    g_camera_service_send_ok = (statusRet == EC_SUCCESS &&
                                selfTestRet == EC_SUCCESS &&
                                captureRet == EC_SUCCESS &&
                                streamRet == EC_SUCCESS);

    esp_rom_printf("[OHOS-S46A] CameraService send status=%d selfTest=%d capture=%d stream=%d sendOk=%u initOk=%u queueOk=%u ready=%u\n",
                   statusRet,
                   selfTestRet,
                   captureRet,
                   streamRet,
                   g_camera_service_send_ok,
                   initOk,
                   queueOk,
                   ready);

    for (int i = 0; i < 200; ++i) {
        if (g_camera_service_msg_count >= 4 &&
            g_camera_service_status_ok &&
            g_camera_service_selftest_ok &&
            g_camera_service_capture_ok &&
            g_camera_service_stream_ok) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL msgOk = (g_camera_service_msg_count >= 4 &&
                  g_camera_service_status_ok &&
                  g_camera_service_selftest_ok &&
                  g_camera_service_capture_ok &&
                  g_camera_service_stream_ok);

    BOOL ok = (initOk &&
               queueOk &&
               ready &&
               g_camera_service_send_ok &&
               msgOk);

    esp_rom_printf("[OHOS-S46A] CameraService verify init=%u queue=%u ready=%u send=%u msg=%u msgCnt=%u status=%u selfTest=%u capture=%u stream=%u ok=%u\n",
                   initOk,
                   queueOk,
                   ready,
                   g_camera_service_send_ok,
                   msgOk,
                   g_camera_service_msg_count,
                   g_camera_service_status_ok,
                   g_camera_service_selftest_ok,
                   g_camera_service_capture_ok,
                   g_camera_service_stream_ok,
                   ok);

    return NULL;
}


uint32_t OhosCameraServiceSelfTest(void)
{
    uint32_t ready = OhosServiceWaitReady(&g_camera_service_init_count,
                                          &g_camera_service_identity,
                                          200);
    uint32_t baseMsgCnt = g_camera_service_msg_count;

    int32 ret0 = EC_FAILURE;
    int32 ret1 = EC_FAILURE;
    int32 ret2 = EC_FAILURE;
    int32 ret3 = EC_FAILURE;

    if (ready) {
        ret0 = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_GET_STATUS,
                                 OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        ret1 = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_SELF_TEST,
                                 OHOS_SERVICE_CMD_SELF_TEST);
        LOS_TaskDelay(10);

        ret2 = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_CAPTURE_TEST,
                                 OHOS_SERVICE_CMD_START);
        LOS_TaskDelay(10);

        ret3 = CameraServiceSendRequest(OHOS_SERVICE_MSG_CAMERA_STREAM_TEST,
                                 OHOS_SERVICE_CMD_START);
        LOS_TaskDelay(10);
    }

    for (int i = 0; i < 200; ++i) {
        if (g_camera_service_msg_count >= baseMsgCnt + 4) {
            break;
        }
        LOS_TaskDelay(1);
    }

    uint32_t msgOk = (g_camera_service_msg_count >= baseMsgCnt + 4);
    uint32_t sendOk = (ret0 == EC_SUCCESS &&
                       ret1 == EC_SUCCESS &&
                       ret2 == EC_SUCCESS &&
                       ret3 == EC_SUCCESS);
    uint32_t ok = (ready && sendOk && msgOk);

    esp_rom_printf("[OHOS-S46A] CameraService selftest ready=%u send=%u msg=%u base=%u now=%u ok=%u\n",
                   ready,
                   sendOk,
                   msgOk,
                   baseMsgCnt,
                   g_camera_service_msg_count,
                   ok);

    return ok ? LOS_OK : 1U;
}


extern void OhosCameraPreviewStartLvTimer(void);
extern void OhosCameraCaptureStreamProbe(void);

static volatile uint32_t g_camera_real_preview_started = 0;
static UINT32 g_camera_real_preview_task_id = 0;

static void OhosCameraServiceRealPreviewTask(void *arg)
{
    (void)arg;
    esp_rom_printf("[OHOS-CAMERA-HW] CameraService real preview task start\n");
    OhosCameraCaptureStreamProbe();
    esp_rom_printf("[OHOS-CAMERA-HW] CameraService real preview task finished\n");
    g_camera_real_preview_started = 0;
    return;
}

uint32_t OhosCameraServiceStartRealPreview(void)
{
    if (g_camera_real_preview_started) {
        esp_rom_printf("[OHOS-CAMERA-HW] CameraService real preview already started\n");
        return 0;
    }

    esp_rom_printf("[OHOS-CAMERA-HW] CameraService start LVGL timer + SC2336 preview\n");
    OhosCameraPreviewStartLvTimer();

    UINT32 ret = OhosLiteosCreateTask("ohos_cam_service_preview",
                                      OhosCameraServiceRealPreviewTask,
                                      NULL,
                                      OHOS_CAMERA_PREVIEW_TASK_PRIO,
                                      OHOS_CAMERA_PREVIEW_TASK_STACK,
                                      &g_camera_real_preview_task_id);

    esp_rom_printf("[OHOS-CAMERA-HW] CameraService preview LiteOS task create ret=%u taskId=%u\n",
                   ret,
                   g_camera_real_preview_task_id);

    if (ret != LOS_OK) {
        return 1;
    }

    g_camera_real_preview_started = 1;
    return 0;
}

uint32_t OhosCameraServiceRegister(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    if (samgr == NULL) {
        esp_rom_printf("[OHOS-S46A] CameraService register failed: samgr null\n");
        return 1U;
    }

    BOOL regOk = samgr->RegisterService(&g_camera_service);

    esp_rom_printf("[OHOS-S46A] CameraService register regOk=%u\n", regOk);

    return (regOk == TRUE) ? LOS_OK : 2U;
}

uint32_t OhosCameraServiceStartTasks(void)
{
    esp_rom_printf("[OHOS-S46A] CameraService standalone verify task skipped, unified selftest will run\n");
    return LOS_OK;
}

uint32_t OhosCameraServiceStart(void)
{
    uint32_t ret = OhosCameraServiceRegister();

    if (ret != LOS_OK) {
        return ret;
    }

    SAMGR_Bootstrap();

    ret = OhosCameraServiceStartTasks();

    return ret;
}

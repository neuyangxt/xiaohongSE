#include <stdint.h>

#include "ohos_types.h"
#include "ohos_errno.h"
#include "los_task.h"
#include "samgr_lite.h"
#include "service_impl.h"
#include "ohos_service_msg.h"
#include "ohos_service_common.h"
#include "esp_rom_sys.h"
#include "ohos_audio_es8311_port.h"
#include "ohos_audio_es7210_port.h"
#include "voice_command_service.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#define OHOS_AUDIO_SERVICE_NAME "AudioService"

#define OHOS_AUDIO_VERIFY_TASK_PRIO   25
#define OHOS_AUDIO_VERIFY_TASK_STACK  0x1000

static volatile uint32_t g_audio_service_init_count = 0;
static volatile uint32_t g_audio_service_msg_count = 0;
static volatile uint32_t g_audio_service_queue_ok = 0;
static volatile uint32_t g_audio_service_send_ok = 0;
static volatile uint32_t g_audio_service_status_ok = 0;
static volatile uint32_t g_audio_service_selftest_ok = 0;
static volatile uint32_t g_audio_service_play_ok = 0;
static volatile uint32_t g_audio_service_record_ok = 0;


extern uint32_t OhosAudioEs8311PortStopPlay(void);

uint32_t OhosAudioServiceStartRealHw(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService start real ES8311 audio hw\n");
    uint32_t ret = OhosAudioEs8311PortStartRealHw();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService real hw ret=%u\n", ret);
    return ret;
}

uint32_t OhosAudioServicePlayTest(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService play test start\n");
    uint32_t ret = OhosAudioEs8311PortPlayOnce();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService play test ret=%u\n", ret);
    return ret;
}


uint32_t OhosAudioServiceStopSpeaker(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService speaker stop\n");
    uint32_t ret = OhosAudioEs8311PortStopPlay();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService speaker stop ret=%u\n", ret);
    return ret;
}

uint32_t OhosAudioServiceRecordStatsTest(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210 mic record stats start\n");
    uint32_t ret = OhosAudioEs7210StartRecordStatsTest();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210 mic record stats ret=%u\n", ret);
    return ret;
}



uint32_t OhosAudioServiceStopMicStats(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210 mic record stats stop\n");
    uint32_t ret = OhosAudioEs7210StopRecordStats();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210 mic record stats stop ret=%u\n", ret);
    return ret;
}

uint32_t OhosAudioServiceLoopbackTest(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210->ES8311 loopback test start\n");
    uint32_t ret = OhosAudioEs7210StartLoopbackTest();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210->ES8311 loopback test ret=%u\n", ret);
    return ret;
}



uint32_t OhosAudioServiceStopLoopback(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210->ES8311 loopback stop\n");
    uint32_t ret = OhosAudioEs7210StopLoopbackTest();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService ES7210->ES8311 loopback stop ret=%u\n", ret);
    return ret;
}

uint32_t OhosAudioServiceCombinedSelfTest(void)
{
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService combined play+record selftest start\n");
    uint32_t ret = OhosAudioEs8311PortCombinedSelfTest();
    esp_rom_printf("[OHOS-AUDIO-HW] AudioService combined play+record selftest ret=%u\n", ret);
    return ret;
}

static Identity g_audio_service_identity = {
    .serviceId = -1,
    .featureId = -1,
    .queueId = NULL,
};

static const char *AudioServiceGetName(Service *service)
{
    (void)service;
    return OHOS_AUDIO_SERVICE_NAME;
}

static BOOL AudioServiceInitialize(Service *service, Identity identity)
{
    (void)service;

    g_audio_service_init_count++;
    g_audio_service_identity = identity;
    g_audio_service_queue_ok = (identity.queueId != NULL);

    /*
     * S44A only validates AudioService framework.
     * Real I2S / ES8311 / ES7210 driver will be connected in later stages.
     */
    esp_rom_printf("[OHOS-S46A] AudioService init count=%u serviceId=%d featureId=%d queue=%p queueOk=%u\n",
                   g_audio_service_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId,
                   g_audio_service_queue_ok);

    return TRUE;
}

static BOOL AudioServiceMessageHandle(Service *service, Request *request)
{
    (void)service;

    g_audio_service_msg_count++;

    BOOL reqOk = FALSE;
    int msgId = request ? request->msgId : -1;
    uint32_t cmd = request ? request->msgValue : 0xffffffffU;

    if (request != NULL) {
        if (request->msgId == OHOS_SERVICE_MSG_AUDIO_GET_STATUS &&
            request->msgValue == OHOS_SERVICE_CMD_GET_STATUS) {
            g_audio_service_status_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_AUDIO_SELF_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_SELF_TEST) {
            g_audio_service_selftest_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_AUDIO_PLAY_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_START) {
            g_audio_service_play_ok = TRUE;
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_AUDIO_RECORD_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_START) {
            g_audio_service_record_ok = TRUE;
            reqOk = TRUE;
        }
    }

    esp_rom_printf("[OHOS-S46A] AudioService message count=%u req=%p msgId=%d cmd=%u reqOk=%u statusOk=%u selfTestOk=%u playOk=%u recordOk=%u\n",
                   g_audio_service_msg_count,
                   request,
                   msgId,
                   cmd,
                   reqOk,
                   g_audio_service_status_ok,
                   g_audio_service_selftest_ok,
                   g_audio_service_play_ok,
                   g_audio_service_record_ok);

    return TRUE;
}

static TaskConfig AudioServiceGetTaskConfig(Service *service)
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

static Service g_audio_service = {
    .GetName = AudioServiceGetName,
    .Initialize = AudioServiceInitialize,
    .MessageHandle = AudioServiceMessageHandle,
    .GetTaskConfig = AudioServiceGetTaskConfig,
};

static int32 AudioServiceSendRequest(int16 msgId, uint32_t cmd)
{
    return OhosServiceSendSimpleRequest(&g_audio_service_identity, msgId, cmd);
}

static VOID *OhosAudioServiceVerifyTask(UINT32 arg)
{
    (void)arg;

    BOOL ready = OhosServiceWaitReady(&g_audio_service_init_count,
                                      &g_audio_service_identity,
                                      200);

    BOOL initOk = (g_audio_service_init_count > 0);
    BOOL queueOk = (g_audio_service_identity.queueId != NULL);

    int32 statusRet = EC_FAILURE;
    int32 selfTestRet = EC_FAILURE;
    int32 playRet = EC_FAILURE;
    int32 recordRet = EC_FAILURE;

    if (ready) {
        statusRet = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_GET_STATUS,
                                            OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        selfTestRet = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_SELF_TEST,
                                              OHOS_SERVICE_CMD_SELF_TEST);
        LOS_TaskDelay(10);

        playRet = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_PLAY_TEST,
                                          OHOS_SERVICE_CMD_START);
        LOS_TaskDelay(10);

        recordRet = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_RECORD_TEST,
                                            OHOS_SERVICE_CMD_START);
    }

    g_audio_service_send_ok = (statusRet == EC_SUCCESS &&
                               selfTestRet == EC_SUCCESS &&
                               playRet == EC_SUCCESS &&
                               recordRet == EC_SUCCESS);

    esp_rom_printf("[OHOS-S46A] AudioService send status=%d selfTest=%d play=%d record=%d sendOk=%u initOk=%u queueOk=%u ready=%u\n",
                   statusRet,
                   selfTestRet,
                   playRet,
                   recordRet,
                   g_audio_service_send_ok,
                   initOk,
                   queueOk,
                   ready);

    for (int i = 0; i < 200; ++i) {
        if (g_audio_service_msg_count >= 4 &&
            g_audio_service_status_ok &&
            g_audio_service_selftest_ok &&
            g_audio_service_play_ok &&
            g_audio_service_record_ok) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL msgOk = (g_audio_service_msg_count >= 4 &&
                  g_audio_service_status_ok &&
                  g_audio_service_selftest_ok &&
                  g_audio_service_play_ok &&
                  g_audio_service_record_ok);

    BOOL ok = (initOk &&
               queueOk &&
               ready &&
               g_audio_service_send_ok &&
               msgOk);

    esp_rom_printf("[OHOS-S46A] AudioService verify init=%u queue=%u ready=%u send=%u msg=%u msgCnt=%u status=%u selfTest=%u play=%u record=%u ok=%u\n",
                   initOk,
                   queueOk,
                   ready,
                   g_audio_service_send_ok,
                   msgOk,
                   g_audio_service_msg_count,
                   g_audio_service_status_ok,
                   g_audio_service_selftest_ok,
                   g_audio_service_play_ok,
                   g_audio_service_record_ok,
                   ok);

    return NULL;
}


uint32_t OhosAudioServiceSelfTest(void)
{
    uint32_t ready = OhosServiceWaitReady(&g_audio_service_init_count,
                                          &g_audio_service_identity,
                                          200);
    uint32_t baseMsgCnt = g_audio_service_msg_count;

    int32 ret0 = EC_FAILURE;
    int32 ret1 = EC_FAILURE;
    int32 ret2 = EC_FAILURE;
    int32 ret3 = EC_FAILURE;

    if (ready) {
        ret0 = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_GET_STATUS,
                                 OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        ret1 = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_SELF_TEST,
                                 OHOS_SERVICE_CMD_SELF_TEST);
        LOS_TaskDelay(10);

        ret2 = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_PLAY_TEST,
                                 OHOS_SERVICE_CMD_START);
        LOS_TaskDelay(10);

        ret3 = AudioServiceSendRequest(OHOS_SERVICE_MSG_AUDIO_RECORD_TEST,
                                 OHOS_SERVICE_CMD_START);
        LOS_TaskDelay(10);
    }

    for (int i = 0; i < 200; ++i) {
        if (g_audio_service_msg_count >= baseMsgCnt + 4) {
            break;
        }
        LOS_TaskDelay(1);
    }

    uint32_t msgOk = (g_audio_service_msg_count >= baseMsgCnt + 4);
    uint32_t sendOk = (ret0 == EC_SUCCESS &&
                       ret1 == EC_SUCCESS &&
                       ret2 == EC_SUCCESS &&
                       ret3 == EC_SUCCESS);
    uint32_t ok = (ready && sendOk && msgOk);

    esp_rom_printf("[OHOS-S46A] AudioService selftest ready=%u send=%u msg=%u base=%u now=%u ok=%u\n",
                   ready,
                   sendOk,
                   msgOk,
                   baseMsgCnt,
                   g_audio_service_msg_count,
                   ok);

    return ok ? LOS_OK : 1U;
}

uint32_t OhosAudioServiceRegister(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    if (samgr == NULL) {
        esp_rom_printf("[OHOS-S46A] AudioService register failed: samgr null\n");
        return 1U;
    }

    BOOL regOk = samgr->RegisterService(&g_audio_service);

    esp_rom_printf("[OHOS-S46A] AudioService register regOk=%u\n", regOk);

    return (regOk == TRUE) ? LOS_OK : 2U;
}

uint32_t OhosAudioServiceStartTasks(void)
{
    esp_rom_printf("[OHOS-S46A] AudioService standalone verify task skipped, unified selftest will run\n");
    return OhosVoiceCommandServiceStart();
}

uint32_t OhosAudioServiceStart(void)
{
    uint32_t ret = OhosAudioServiceRegister();

    if (ret != LOS_OK) {
        return ret;
    }

    SAMGR_Bootstrap();

    ret = OhosAudioServiceStartTasks();

    return ret;
}

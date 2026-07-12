#include <stdint.h>

#include "ohos_types.h"
#include "ohos_errno.h"
#include "los_task.h"
#include "samgr_lite.h"
#include "service_impl.h"
#include "ohos_service_msg.h"
#include "ohos_service_common.h"
#include "ohos_uart_link_ui.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "driver/gpio.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#define OHOS_KEY_SERVICE_NAME "KeyService"

/* DNESP32P4 BOOT key: GPIO35, pull-up input, active-low after boot */
#define OHOS_KEY_BOOT_GPIO       GPIO_NUM_35
#define OHOS_KEY_K2_GPIO         GPIO_NUM_1
#define OHOS_KEY_K2_VERIFY_ENABLE 1U
#define OHOS_KEY_LEVEL_PRESSED   0U
#define OHOS_KEY_LEVEL_RELEASED  1U

/* LED0: GPIO51, active-low */
#define OHOS_KEY_FEEDBACK_LED_GPIO       GPIO_NUM_51
#define OHOS_KEY_FEEDBACK_LED_LEVEL_ON   0U
#define OHOS_KEY_FEEDBACK_LED_LEVEL_OFF  1U

#define OHOS_KEY_VERIFY_TASK_PRIO     25
#define OHOS_KEY_VERIFY_TASK_STACK    0x1000

#define OHOS_KEY_MONITOR_TASK_PRIO    26
#define OHOS_KEY_MONITOR_TASK_STACK   0x1000
#define OHOS_KEY_K2_ACTION_TASK_PRIO  24
#define OHOS_KEY_K2_ACTION_TASK_STACK 0x2000
#define OHOS_KEY_POLL_DELAY_TICKS     5
#define OHOS_KEY_DEBOUNCE_COUNT       3
#define OHOS_KEY_K2_VERIFY_POLL_TICKS 1
#define OHOS_KEY_K2_VERIFY_DEBOUNCE   1
#define OHOS_KEY_K2_VERIFY_GUARD_MS   30
#define OHOS_KEY_K2_VOICE_ACTION_ENABLE 1U
#define OHOS_KEY_K2_ACTION_COOLDOWN_MS 600U

static volatile uint32_t g_key_service_init_count = 0;
static volatile uint32_t g_key_service_msg_count = 0;
static volatile uint32_t g_key_service_queue_ok = 0;
static volatile uint32_t g_key_service_send_ok = 0;
static volatile uint32_t g_key_service_msg_ok = 0;
static volatile uint32_t g_key_service_last_level = 1;
static volatile uint32_t g_key_service_last_pressed = 0;
static volatile uint32_t g_key_service_event_count = 0;
static volatile uint32_t g_key_service_k2_last_level = 1;
static volatile uint32_t g_key_service_k2_last_pressed = 0;
static volatile uint32_t g_key_service_k2_event_count = 0;
static volatile uint32_t g_key_service_k2_press_start_ms = 0;
static volatile uint32_t g_key_service_k2_voice_action_count = 0;
static volatile uint32_t g_key_service_k2_voice_action_ok = 0;
static volatile uint32_t g_key_service_k2_voice_action_reject = 0;
static volatile uint32_t g_key_service_k2_voice_action_last_duration_ms = 0;
static volatile uint32_t g_key_service_k2_voice_action_last_ms = 0;

static Identity g_key_service_identity = {
    .serviceId = -1,
    .featureId = -1,
    .queueId = NULL,
};

static const char *KeyServiceGetName(Service *service)
{
    (void)service;
    return OHOS_KEY_SERVICE_NAME;
}

static void KeyServiceFeedbackLedInit(void)
{
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << OHOS_KEY_FEEDBACK_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t cfgRet = gpio_config(&led_conf);
    esp_err_t setRet = gpio_set_level(OHOS_KEY_FEEDBACK_LED_GPIO,
                                      OHOS_KEY_FEEDBACK_LED_LEVEL_OFF);

    esp_rom_printf("[OHOS-S46A] KeyService feedback LED init gpio=%d cfgRet=%d setRet=%d level=%u\n",
                   OHOS_KEY_FEEDBACK_LED_GPIO,
                   cfgRet,
                   setRet,
                   OHOS_KEY_FEEDBACK_LED_LEVEL_OFF);
}

static void KeyServiceFeedbackLedSet(uint32_t pressed)
{
    uint32_t level = pressed ? OHOS_KEY_FEEDBACK_LED_LEVEL_ON
                             : OHOS_KEY_FEEDBACK_LED_LEVEL_OFF;

    gpio_set_level(OHOS_KEY_FEEDBACK_LED_GPIO, level);

    esp_rom_printf("[OHOS-S46A] KeyService feedback LED set pressed=%u level=%u\n",
                   pressed,
                   level);
}

#if OHOS_KEY_K2_VERIFY_ENABLE
static void KeyServiceK2VerifyGpioInit(void)
{
    gpio_config_t k2_conf = {
        .pin_bit_mask = (1ULL << OHOS_KEY_K2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t k2CfgRet = gpio_config(&k2_conf);
    int k2Level = gpio_get_level(OHOS_KEY_K2_GPIO);

    g_key_service_k2_last_level = (uint32_t)k2Level;
    g_key_service_k2_last_pressed = (k2Level == OHOS_KEY_LEVEL_PRESSED);
    g_key_service_k2_press_start_ms = g_key_service_k2_last_pressed ?
                                      (uint32_t)esp_log_timestamp() : 0U;

    esp_rom_printf("[P4-K2] verify gpio init gpio=%d cfgRet=%d level=%u pressed=%u pullup=1 action=voice_user_action\n",
                   OHOS_KEY_K2_GPIO,
                   k2CfgRet,
                   g_key_service_k2_last_level,
                   g_key_service_k2_last_pressed);
}
#endif

static void KeyServiceGpioInit(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OHOS_KEY_BOOT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t cfgRet = gpio_config(&io_conf);

    int level = gpio_get_level(OHOS_KEY_BOOT_GPIO);
    g_key_service_last_level = (uint32_t)level;
    g_key_service_last_pressed = (level == OHOS_KEY_LEVEL_PRESSED);

    esp_rom_printf("[OHOS-S46A] KeyService gpio init gpio=%d cfgRet=%d level=%u pressed=%u\n",
                   OHOS_KEY_BOOT_GPIO,
                   cfgRet,
                   g_key_service_last_level,
                   g_key_service_last_pressed);

#if OHOS_KEY_K2_VERIFY_ENABLE
    KeyServiceK2VerifyGpioInit();
#endif

    KeyServiceFeedbackLedInit();
}

static uint32_t KeyServiceReadPressed(void)
{
    int level = gpio_get_level(OHOS_KEY_BOOT_GPIO);

    g_key_service_last_level = (uint32_t)level;
    g_key_service_last_pressed = (level == OHOS_KEY_LEVEL_PRESSED);

    return g_key_service_last_pressed;
}

static BOOL KeyServiceInitialize(Service *service, Identity identity)
{
    (void)service;

    g_key_service_init_count++;
    g_key_service_identity = identity;
    g_key_service_queue_ok = (identity.queueId != NULL);

    KeyServiceGpioInit();

    esp_rom_printf("[OHOS-S46A] KeyService init count=%u serviceId=%d featureId=%d queue=%p queueOk=%u\n",
                   g_key_service_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId,
                   g_key_service_queue_ok);

    return TRUE;
}

static BOOL KeyServiceMessageHandle(Service *service, Request *request)
{
    (void)service;

    g_key_service_msg_count++;

    BOOL reqOk = FALSE;
    uint32_t pressed = KeyServiceReadPressed();

    if (request != NULL) {
        if (request->msgId == OHOS_SERVICE_MSG_KEY_GET_STATE &&
            request->msgValue == OHOS_SERVICE_CMD_GET_STATUS) {
            reqOk = TRUE;
        } else if (request->msgId == OHOS_SERVICE_MSG_KEY_SELF_TEST &&
                   request->msgValue == OHOS_SERVICE_CMD_SELF_TEST) {
            reqOk = TRUE;
        }
    }

    if (reqOk) {
        g_key_service_msg_ok = TRUE;
    }

    esp_rom_printf("[OHOS-S46A] KeyService message count=%u req=%p msgId=%d cmd=%u reqOk=%u level=%u pressed=%u\n",
                   g_key_service_msg_count,
                   request,
                   request ? request->msgId : -1,
                   request ? request->msgValue : 0xffffffffU,
                   reqOk,
                   g_key_service_last_level,
                   pressed);

    return TRUE;
}

static TaskConfig KeyServiceGetTaskConfig(Service *service)
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

static Service g_key_service = {
    .GetName = KeyServiceGetName,
    .Initialize = KeyServiceInitialize,
    .MessageHandle = KeyServiceMessageHandle,
    .GetTaskConfig = KeyServiceGetTaskConfig,
};

static int32 KeyServiceSendRequest(int16 msgId, uint32_t cmd)
{
    return OhosServiceSendSimpleRequest(&g_key_service_identity, msgId, cmd);
}

static VOID *OhosKeyServiceVerifyTask(UINT32 arg)
{
    (void)arg;

    for (int i = 0; i < 200; ++i) {
        if (g_key_service_init_count > 0 && g_key_service_identity.queueId != NULL) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL initOk = (g_key_service_init_count > 0);
    BOOL queueOk = (g_key_service_identity.queueId != NULL);

    int32 getStateRet = EC_FAILURE;
    int32 selfTestRet = EC_FAILURE;

    if (initOk && queueOk) {
        getStateRet = KeyServiceSendRequest(OHOS_SERVICE_MSG_KEY_GET_STATE,
                                            OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        selfTestRet = KeyServiceSendRequest(OHOS_SERVICE_MSG_KEY_SELF_TEST,
                                            OHOS_SERVICE_CMD_SELF_TEST);
    }

    g_key_service_send_ok = (getStateRet == EC_SUCCESS &&
                             selfTestRet == EC_SUCCESS);

    esp_rom_printf("[OHOS-S46A] KeyService send getState=%d selfTest=%d sendOk=%u initOk=%u queueOk=%u\n",
                   getStateRet,
                   selfTestRet,
                   g_key_service_send_ok,
                   initOk,
                   queueOk);

    for (int i = 0; i < 200; ++i) {
        if (g_key_service_msg_count >= 2 && g_key_service_msg_ok == TRUE) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL msgOk = (g_key_service_msg_count >= 2 && g_key_service_msg_ok == TRUE);

    BOOL ok = (initOk && queueOk && g_key_service_send_ok && msgOk);

    esp_rom_printf("[OHOS-S46A] KeyService verify init=%u queue=%u send=%u msg=%u msgCnt=%u level=%u pressed=%u ok=%u\n",
                   initOk,
                   queueOk,
                   g_key_service_send_ok,
                   msgOk,
                   g_key_service_msg_count,
                   g_key_service_last_level,
                   g_key_service_last_pressed,
                   ok);

    return NULL;
}

#if OHOS_KEY_K2_VERIFY_ENABLE
static void KeyServiceK2TriggerVoiceAction(const char *eventName, uint32_t nowMs, uint32_t durationMs)
{
#if OHOS_KEY_K2_VOICE_ACTION_ENABLE
    uint32_t ageMs = (g_key_service_k2_voice_action_last_ms == 0U) ?
                     0xFFFFFFFFU :
                     (uint32_t)(nowMs - g_key_service_k2_voice_action_last_ms);

    g_key_service_k2_voice_action_last_duration_ms = durationMs;

    if (g_key_service_k2_voice_action_last_ms != 0U &&
        ageMs < OHOS_KEY_K2_ACTION_COOLDOWN_MS) {
        g_key_service_k2_voice_action_reject++;
        esp_rom_printf("[P4-K2] voice action cooldown event=%s duration=%u age=%u cooldown=%u reject=%u action=press_edge\n",
                       eventName ? eventName : "-",
                       durationMs,
                       ageMs,
                       OHOS_KEY_K2_ACTION_COOLDOWN_MS,
                       g_key_service_k2_voice_action_reject);
        return;
    }

    g_key_service_k2_voice_action_count++;
    g_key_service_k2_voice_action_last_ms = nowMs;
    uint32_t ok = OhosUartLinkUiVoiceUserAction();
    if (ok) {
        g_key_service_k2_voice_action_ok++;
    }

    esp_rom_printf("[P4-K2] voice action event=%s duration=%u ok=%u action=%u okCnt=%u reject=%u age=%u cooldown=%u\n",
                   eventName ? eventName : "-",
                   durationMs,
                   ok,
                   g_key_service_k2_voice_action_count,
                   g_key_service_k2_voice_action_ok,
                   g_key_service_k2_voice_action_reject,
                   ageMs,
                   OHOS_KEY_K2_ACTION_COOLDOWN_MS);
#else
    (void)eventName;
    (void)nowMs;
    (void)durationMs;
#endif
}

static VOID *OhosKeyServiceK2VerifyOnlyTask(UINT32 arg)
{
    (void)arg;

    KeyServiceK2VerifyGpioInit();

    uint32_t k2StableLevel = g_key_service_k2_last_level;
    uint32_t k2LastRawLevel = k2StableLevel;
    uint32_t k2LastEventMs = 0U;

    esp_rom_printf("[P4-K2] voice-action monitor entered gpio=%d initialLevel=%u pressed=%u debounce=%u pollTicks=%u guardMs=%u action=voice_user_action press_edge release_log cooldown=%u\n",
                   OHOS_KEY_K2_GPIO,
                   k2StableLevel,
                   g_key_service_k2_last_pressed,
                   OHOS_KEY_K2_VERIFY_DEBOUNCE,
                   OHOS_KEY_K2_VERIFY_POLL_TICKS,
                   OHOS_KEY_K2_VERIFY_GUARD_MS,
                   OHOS_KEY_K2_ACTION_COOLDOWN_MS);

    while (1) {
        uint32_t k2RawLevel = (uint32_t)gpio_get_level(OHOS_KEY_K2_GPIO);

        if (k2RawLevel != k2LastRawLevel) {
            uint32_t nowMs = (uint32_t)esp_log_timestamp();
            uint32_t prevRaw = k2LastRawLevel;
            uint32_t guardOk = (k2LastEventMs == 0U) ||
                               ((uint32_t)(nowMs - k2LastEventMs) >= OHOS_KEY_K2_VERIFY_GUARD_MS);

            k2LastRawLevel = k2RawLevel;
            esp_rom_printf("[P4-K2] raw change raw=%u prevRaw=%u stable=%u guardOk=%u action=voice_user_action\n",
                           k2RawLevel,
                           prevRaw,
                           k2StableLevel,
                           guardOk);

            if (guardOk && k2RawLevel != k2StableLevel) {
                uint32_t prevStable = k2StableLevel;
                uint32_t durationMs = 0U;
                const char *eventName = (k2RawLevel == OHOS_KEY_LEVEL_PRESSED) ?
                                        "press" : "release";

                k2StableLevel = k2RawLevel;
                g_key_service_k2_last_level = k2StableLevel;
                g_key_service_k2_last_pressed = (k2StableLevel == OHOS_KEY_LEVEL_PRESSED);
                g_key_service_k2_event_count++;
                k2LastEventMs = nowMs;

                if (g_key_service_k2_last_pressed) {
                    g_key_service_k2_press_start_ms = nowMs;
                } else if (g_key_service_k2_press_start_ms != 0U) {
                    durationMs = (uint32_t)(nowMs - g_key_service_k2_press_start_ms);
                    g_key_service_k2_press_start_ms = 0U;
                }

                esp_rom_printf("[P4-K2] verify-only event=%s count=%u raw=%u prev=%u stable=%u pressed=%u duration=%u guardMs=%u action=voice_user_action\n",
                               eventName,
                               g_key_service_k2_event_count,
                               k2RawLevel,
                               prevStable,
                               g_key_service_k2_last_level,
                               g_key_service_k2_last_pressed,
                               durationMs,
                               OHOS_KEY_K2_VERIFY_GUARD_MS);

                if (k2RawLevel == OHOS_KEY_LEVEL_PRESSED) {
                    KeyServiceK2TriggerVoiceAction("press", nowMs, 0U);
                }
            }
        }

        LOS_TaskDelay(OHOS_KEY_K2_VERIFY_POLL_TICKS);
    }

    return NULL;
}
#endif

static VOID *OhosKeyServiceMonitorTask(UINT32 arg)
{
    (void)arg;

    for (int i = 0; i < 200; ++i) {
        if (g_key_service_init_count > 0) {
            break;
        }
        LOS_TaskDelay(1);
    }

    uint32_t stableLevel = g_key_service_last_level;
    uint32_t lastRawLevel = stableLevel;
    uint32_t sameCount = 0;
#if OHOS_KEY_K2_VERIFY_ENABLE
    uint32_t k2StableLevel = g_key_service_k2_last_level;
    uint32_t k2LastRawLevel = k2StableLevel;
    uint32_t k2SameCount = 0;
#endif

    esp_rom_printf("[OHOS-S46A] KeyService monitor entered gpio=%d initialLevel=%u pressed=%u\n",
                   OHOS_KEY_BOOT_GPIO,
                   stableLevel,
                   g_key_service_last_pressed);
#if OHOS_KEY_K2_VERIFY_ENABLE
    esp_rom_printf("[P4-K2] verify monitor entered gpio=%d initialLevel=%u pressed=%u debounce=%u action=log_only\n",
                   OHOS_KEY_K2_GPIO,
                   k2StableLevel,
                   g_key_service_k2_last_pressed,
                   OHOS_KEY_DEBOUNCE_COUNT);
#endif

    while (1) {
        uint32_t rawLevel = (uint32_t)gpio_get_level(OHOS_KEY_BOOT_GPIO);

        if (rawLevel == lastRawLevel) {
            if (sameCount < OHOS_KEY_DEBOUNCE_COUNT) {
                sameCount++;
            }
        } else {
            sameCount = 0;
            lastRawLevel = rawLevel;
        }

        if (sameCount >= OHOS_KEY_DEBOUNCE_COUNT && rawLevel != stableLevel) {
            stableLevel = rawLevel;
            g_key_service_last_level = stableLevel;
            g_key_service_last_pressed = (stableLevel == OHOS_KEY_LEVEL_PRESSED);
            g_key_service_event_count++;

            esp_rom_printf("[OHOS-S46A] KeyService event count=%u level=%u pressed=%u\n",
                           g_key_service_event_count,
                           g_key_service_last_level,
                           g_key_service_last_pressed);

            KeyServiceFeedbackLedSet(g_key_service_last_pressed);
        }

#if OHOS_KEY_K2_VERIFY_ENABLE
        uint32_t k2RawLevel = (uint32_t)gpio_get_level(OHOS_KEY_K2_GPIO);

        if (k2RawLevel == k2LastRawLevel) {
            if (k2SameCount < OHOS_KEY_DEBOUNCE_COUNT) {
                k2SameCount++;
            }
        } else {
            k2SameCount = 0;
            k2LastRawLevel = k2RawLevel;
            esp_rom_printf("[P4-K2] raw change raw=%u stable=%u same=%u\n",
                           k2RawLevel,
                           k2StableLevel,
                           k2SameCount);
        }

        if (k2SameCount >= OHOS_KEY_DEBOUNCE_COUNT && k2RawLevel != k2StableLevel) {
            uint32_t nowMs = (uint32_t)esp_log_timestamp();
            uint32_t prevStable = k2StableLevel;
            uint32_t durationMs = 0U;

            k2StableLevel = k2RawLevel;
            g_key_service_k2_last_level = k2StableLevel;
            g_key_service_k2_last_pressed = (k2StableLevel == OHOS_KEY_LEVEL_PRESSED);
            g_key_service_k2_event_count++;

            if (g_key_service_k2_last_pressed) {
                g_key_service_k2_press_start_ms = nowMs;
            } else if (g_key_service_k2_press_start_ms != 0U) {
                durationMs = (uint32_t)(nowMs - g_key_service_k2_press_start_ms);
                g_key_service_k2_press_start_ms = 0U;
            }

            esp_rom_printf("[P4-K2] verify event count=%u raw=%u prev=%u stable=%u pressed=%u duration=%u action=log_only\n",
                           g_key_service_k2_event_count,
                           k2RawLevel,
                           prevStable,
                           g_key_service_k2_last_level,
                           g_key_service_k2_last_pressed,
                           durationMs);
        }
#endif

        LOS_TaskDelay(OHOS_KEY_POLL_DELAY_TICKS);
    }

    return NULL;
}



uint32_t OhosKeyServiceSelfTest(void)
{
    uint32_t ready = OhosServiceWaitReady(&g_key_service_init_count,
                                          &g_key_service_identity,
                                          200);
    uint32_t baseMsgCnt = g_key_service_msg_count;

    int32 getStateRet = EC_FAILURE;
    int32 selfTestRet = EC_FAILURE;

    if (ready) {
        getStateRet = KeyServiceSendRequest(OHOS_SERVICE_MSG_KEY_GET_STATE,
                                            OHOS_SERVICE_CMD_GET_STATUS);
        LOS_TaskDelay(10);

        selfTestRet = KeyServiceSendRequest(OHOS_SERVICE_MSG_KEY_SELF_TEST,
                                            OHOS_SERVICE_CMD_SELF_TEST);
    }

    for (int i = 0; i < 200; ++i) {
        if (g_key_service_msg_count >= baseMsgCnt + 2) {
            break;
        }
        LOS_TaskDelay(1);
    }

    uint32_t msgOk = (g_key_service_msg_count >= baseMsgCnt + 2);
    uint32_t sendOk = (getStateRet == EC_SUCCESS &&
                       selfTestRet == EC_SUCCESS);
    uint32_t ok = (ready && sendOk && msgOk);

    esp_rom_printf("[OHOS-S46A] KeyService selftest ready=%u send=%u msg=%u base=%u now=%u level=%u pressed=%u ok=%u\n",
                   ready,
                   sendOk,
                   msgOk,
                   baseMsgCnt,
                   g_key_service_msg_count,
                   g_key_service_last_level,
                   g_key_service_last_pressed,
                   ok);

    return ok ? LOS_OK : 1U;
}

uint32_t OhosKeyServiceRegister(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    if (samgr == NULL) {
        esp_rom_printf("[OHOS-S46A] KeyService register failed: samgr null\n");
        return 1U;
    }

    BOOL regOk = samgr->RegisterService(&g_key_service);

    esp_rom_printf("[OHOS-S46A] KeyService register regOk=%u\n", regOk);

    return (regOk == TRUE) ? LOS_OK : 2U;
}

uint32_t OhosKeyServiceStartTasks(void)
{
    UINT32 monitorTaskId = 0;

    uint32_t ret = OhosServiceCreateTask("[OHOS-S46A]",
                                         "ohos_key_monitor",
                                         OhosKeyServiceMonitorTask,
                                         OHOS_KEY_MONITOR_TASK_PRIO,
                                         OHOS_KEY_MONITOR_TASK_STACK,
                                         0,
                                         &monitorTaskId);

    esp_rom_printf("[OHOS-S46A] KeyService verify task skipped, monitor ret=%u taskId=%u\n",
                   ret,
                   monitorTaskId);

    return ret;
}

uint32_t OhosKeyServiceStartK2VerifyTask(void)
{
#if OHOS_KEY_K2_VERIFY_ENABLE
    UINT32 k2TaskId = 0;

    uint32_t ret = OhosServiceCreateTask("[P4-K2]",
                                         "p4_k2_voice_action",
                                         OhosKeyServiceK2VerifyOnlyTask,
                                         OHOS_KEY_K2_ACTION_TASK_PRIO,
                                         OHOS_KEY_K2_ACTION_TASK_STACK,
                                         0,
                                         &k2TaskId);

    esp_rom_printf("[P4-K2] voice-action task create ret=%u taskId=%u action=voice_user_action press_edge release_log prio=%u cooldown=%u\n",
                   ret,
                   k2TaskId,
                   OHOS_KEY_K2_ACTION_TASK_PRIO,
                   OHOS_KEY_K2_ACTION_COOLDOWN_MS);

    return ret;
#else
    esp_rom_printf("[P4-K2] verify-only task disabled action=log_only\n");
    return LOS_OK;
#endif
}

uint32_t OhosKeyServiceStart(void)
{
    uint32_t ret = OhosKeyServiceRegister();

    if (ret != LOS_OK) {
        return ret;
    }

    SAMGR_Bootstrap();

    ret = OhosKeyServiceStartTasks();

    return ret;
}

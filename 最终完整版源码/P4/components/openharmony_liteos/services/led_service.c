#include <stdint.h>

#include "ohos_types.h"
#include "ohos_errno.h"
#include "los_task.h"
#include "samgr_lite.h"
#include "service_impl.h"
#include "ohos_service_msg.h"
#include "ohos_service_common.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#define OHOS_LED_SERVICE_NAME "LEDService"

#define OHOS_LED_GPIO          GPIO_NUM_51

#define OHOS_LED_LEVEL_ON      0U
#define OHOS_LED_LEVEL_OFF     1U


#define OHOS_LED_VERIFY_TASK_PRIO   25
#define OHOS_LED_VERIFY_TASK_STACK  0x1000

static volatile uint32_t g_led_service_init_count = 0;
static volatile uint32_t g_led_service_msg_count = 0;
static volatile uint32_t g_led_service_queue_ok = 0;
static volatile uint32_t g_led_service_send_ok = 0;
static volatile uint32_t g_led_service_cmd_mask = 0;
static volatile uint32_t g_led_service_last_level = 0;

static Identity g_led_service_identity = {
    .serviceId = -1,
    .featureId = -1,
    .queueId = NULL,
};

static const char *LedServiceGetName(Service *service)
{
    (void)service;
    return OHOS_LED_SERVICE_NAME;
}

static void LedServiceGpioInit(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OHOS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t cfgRet = gpio_config(&io_conf);
    esp_err_t setRet = gpio_set_level(OHOS_LED_GPIO, OHOS_LED_LEVEL_OFF);

    g_led_service_last_level = OHOS_LED_LEVEL_OFF;

    esp_rom_printf("[OHOS-S46A] LEDService gpio init gpio=%d cfgRet=%d setRet=%d level=%u\n",
                   OHOS_LED_GPIO,
                   cfgRet,
                   setRet,
                   g_led_service_last_level);
}

static BOOL LedServiceInitialize(Service *service, Identity identity)
{
    (void)service;

    g_led_service_init_count++;
    g_led_service_identity = identity;
    g_led_service_queue_ok = (identity.queueId != NULL);

    LedServiceGpioInit();

    esp_rom_printf("[OHOS-S46A] LEDService init count=%u serviceId=%d featureId=%d queue=%p queueOk=%u\n",
                   g_led_service_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId,
                   g_led_service_queue_ok);

    return TRUE;
}

static BOOL LedServiceMessageHandle(Service *service, Request *request)
{
    (void)service;

    g_led_service_msg_count++;

    BOOL reqOk = (request != NULL && request->msgId == OHOS_SERVICE_MSG_LED_CONTROL);
    uint32_t cmd = request ? request->msgValue : 0xffffffffU;

    if (reqOk) {
        if (cmd == OHOS_SERVICE_CMD_ON) {
            gpio_set_level(OHOS_LED_GPIO, OHOS_LED_LEVEL_ON);
            g_led_service_last_level = OHOS_LED_LEVEL_ON;
            g_led_service_cmd_mask |= (1U << OHOS_SERVICE_CMD_ON);
        } else if (cmd == OHOS_SERVICE_CMD_OFF) {
            gpio_set_level(OHOS_LED_GPIO, OHOS_LED_LEVEL_OFF);
            g_led_service_last_level = OHOS_LED_LEVEL_OFF;
            g_led_service_cmd_mask |= (1U << OHOS_SERVICE_CMD_OFF);
        } else if (cmd == OHOS_SERVICE_CMD_TOGGLE) {
            int level = gpio_get_level(OHOS_LED_GPIO);
            uint32_t next = level ? OHOS_LED_LEVEL_ON : OHOS_LED_LEVEL_OFF;
            gpio_set_level(OHOS_LED_GPIO, next);
            g_led_service_last_level = next;
            g_led_service_cmd_mask |= (1U << OHOS_SERVICE_CMD_TOGGLE);
        } else {
            reqOk = FALSE;
        }
    }

    esp_rom_printf("[OHOS-S46A] LEDService message count=%u req=%p msgId=%d cmd=%u reqOk=%u level=%u mask=0x%x\n",
                   g_led_service_msg_count,
                   request,
                   request ? request->msgId : -1,
                   cmd,
                   reqOk,
                   g_led_service_last_level,
                   g_led_service_cmd_mask);

    return TRUE;
}

static TaskConfig LedServiceGetTaskConfig(Service *service)
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

static Service g_led_service = {
    .GetName = LedServiceGetName,
    .Initialize = LedServiceInitialize,
    .MessageHandle = LedServiceMessageHandle,
    .GetTaskConfig = LedServiceGetTaskConfig,
};

static int32 LedServiceSendCmd(uint32_t cmd)
{
    return OhosServiceSendSimpleRequest(&g_led_service_identity,
                                        OHOS_SERVICE_MSG_LED_CONTROL,
                                        cmd);
}

static VOID *OhosLedServiceVerifyTask(UINT32 arg)
{
    (void)arg;

    for (int i = 0; i < 200; ++i) {
        if (g_led_service_init_count > 0 && g_led_service_identity.queueId != NULL) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL initOk = (g_led_service_init_count > 0);
    BOOL queueOk = (g_led_service_identity.queueId != NULL);

    int32 sendOnRet = EC_FAILURE;
    int32 sendToggleRet = EC_FAILURE;
    int32 sendOffRet = EC_FAILURE;

    if (initOk && queueOk) {
        sendOnRet = LedServiceSendCmd(OHOS_SERVICE_CMD_ON);
        LOS_TaskDelay(100);

        sendToggleRet = LedServiceSendCmd(OHOS_SERVICE_CMD_TOGGLE);
        LOS_TaskDelay(100);

        sendOffRet = LedServiceSendCmd(OHOS_SERVICE_CMD_OFF);
    }

    g_led_service_send_ok = (sendOnRet == EC_SUCCESS &&
                             sendToggleRet == EC_SUCCESS &&
                             sendOffRet == EC_SUCCESS);

    esp_rom_printf("[OHOS-S46A] LEDService send on=%d toggle=%d off=%d sendOk=%u initOk=%u queueOk=%u\n",
                   sendOnRet,
                   sendToggleRet,
                   sendOffRet,
                   g_led_service_send_ok,
                   initOk,
                   queueOk);

    for (int i = 0; i < 200; ++i) {
        if (g_led_service_msg_count >= 3 &&
            (g_led_service_cmd_mask & (1U << OHOS_SERVICE_CMD_ON)) &&
            (g_led_service_cmd_mask & (1U << OHOS_SERVICE_CMD_TOGGLE)) &&
            (g_led_service_cmd_mask & (1U << OHOS_SERVICE_CMD_OFF))) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL msgOk = (g_led_service_msg_count >= 3 &&
                  (g_led_service_cmd_mask & (1U << OHOS_SERVICE_CMD_ON)) &&
                  (g_led_service_cmd_mask & (1U << OHOS_SERVICE_CMD_TOGGLE)) &&
                  (g_led_service_cmd_mask & (1U << OHOS_SERVICE_CMD_OFF)));

    BOOL ok = (initOk && queueOk && g_led_service_send_ok && msgOk);

    esp_rom_printf("[OHOS-S46A] LEDService verify init=%u queue=%u send=%u msg=%u msgCnt=%u mask=0x%x level=%u ok=%u\n",
                   initOk,
                   queueOk,
                   g_led_service_send_ok,
                   msgOk,
                   g_led_service_msg_count,
                   g_led_service_cmd_mask,
                   g_led_service_last_level,
                   ok);

    return NULL;
}



uint32_t OhosLedServiceSelfTest(void)
{
    uint32_t ready = OhosServiceWaitReady(&g_led_service_init_count,
                                          &g_led_service_identity,
                                          200);
    uint32_t baseMsgCnt = g_led_service_msg_count;

    int32 onRet = EC_FAILURE;
    int32 toggleRet = EC_FAILURE;
    int32 offRet = EC_FAILURE;

    if (ready) {
        onRet = LedServiceSendCmd(OHOS_SERVICE_CMD_ON);
        LOS_TaskDelay(10);

        toggleRet = LedServiceSendCmd(OHOS_SERVICE_CMD_TOGGLE);
        LOS_TaskDelay(10);

        offRet = LedServiceSendCmd(OHOS_SERVICE_CMD_OFF);
    }

    for (int i = 0; i < 200; ++i) {
        if (g_led_service_msg_count >= baseMsgCnt + 3) {
            break;
        }
        LOS_TaskDelay(1);
    }

    uint32_t msgOk = (g_led_service_msg_count >= baseMsgCnt + 3);
    uint32_t sendOk = (onRet == EC_SUCCESS &&
                       toggleRet == EC_SUCCESS &&
                       offRet == EC_SUCCESS);

    uint32_t ok = (ready && sendOk && msgOk);

    esp_rom_printf("[OHOS-S46A] LEDService selftest ready=%u send=%u msg=%u base=%u now=%u ok=%u\n",
                   ready,
                   sendOk,
                   msgOk,
                   baseMsgCnt,
                   g_led_service_msg_count,
                   ok);

    return ok ? LOS_OK : 1U;
}

uint32_t OhosLedServiceRegister(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    if (samgr == NULL) {
        esp_rom_printf("[OHOS-S46A] LEDService register failed: samgr null\n");
        return 1U;
    }

    BOOL regOk = samgr->RegisterService(&g_led_service);

    esp_rom_printf("[OHOS-S46A] LEDService register regOk=%u\n", regOk);

    return (regOk == TRUE) ? LOS_OK : 2U;
}

uint32_t OhosLedServiceStartTasks(void)
{
    esp_rom_printf("[OHOS-S46A] LEDService standalone verify task skipped, unified selftest will run\n");
    return LOS_OK;
}

uint32_t OhosLedServiceStart(void)
{
    uint32_t ret = OhosLedServiceRegister();

    if (ret != LOS_OK) {
        return ret;
    }

    SAMGR_Bootstrap();

    ret = OhosLedServiceStartTasks();

    return ret;
}


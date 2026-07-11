#include <stdint.h>

#include "esp_log.h"
#include "voice_command_service.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

static const char *TAG = "p4_voice_command";

uint32_t OhosVoiceCommandServiceStart(void)
{
    ESP_LOGI(TAG,
             "local AFE/MultiNet disabled; cloud dialog and manual module control remain enabled");
    return LOS_OK;
}

void OhosVoiceCommandAutoVadStart(uint32_t sessionId)
{
    (void)sessionId;
}

void OhosVoiceCommandAutoVadEnd(uint32_t sessionId)
{
    (void)sessionId;
}

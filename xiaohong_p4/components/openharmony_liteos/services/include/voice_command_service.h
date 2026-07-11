#ifndef VOICE_COMMAND_SERVICE_H
#define VOICE_COMMAND_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t OhosVoiceCommandServiceStart(void);
void OhosVoiceCommandAutoVadStart(uint32_t sessionId);
void OhosVoiceCommandAutoVadEnd(uint32_t sessionId);

#ifdef __cplusplus
}
#endif

#endif

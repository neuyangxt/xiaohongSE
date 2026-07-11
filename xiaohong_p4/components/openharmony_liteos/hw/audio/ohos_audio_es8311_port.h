#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t OhosAudioEs8311PortStartRealHw(void);
uint32_t OhosAudioEs8311PortPrepareForDownlink(uint32_t settleMs);
uint32_t OhosAudioEs8311PortPrepareForDialog(uint32_t settleMs);
uint32_t OhosAudioEs8311PortPlayOnce(void);
uint32_t OhosAudioEs8311PortPlayPcm16Mono(const int16_t *pcm, uint32_t samples);
uint32_t OhosAudioEs8311PortPlayPcm16MonoDialog(const int16_t *pcm, uint32_t samples);
uint32_t OhosAudioEs8311PortStartRecordStatsTask(void);
uint32_t OhosAudioEs8311PortCombinedSelfTest(void);
uint32_t OhosAudioEs8311PortStopPlay(void);

#ifdef __cplusplus
}
#endif

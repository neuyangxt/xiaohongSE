#ifndef OHOS_AUDIO_ES7210_PORT_H
#define OHOS_AUDIO_ES7210_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    uint32_t task_started;
    uint32_t seq;
    uint32_t bytes_read;
    uint32_t samples;
    uint32_t peak;
    uint32_t nonzero;
    uint32_t last_ret;
    int32_t first0;
    int32_t first1;
    int32_t first2;
    int32_t first3;
} OhosAudioEs7210RecordStatsSnapshot;

typedef struct {
    uint32_t init_ok;
    uint32_t read_ok;
    uint32_t read_fail;
    uint32_t bytes_read;
    uint32_t samples;
    uint32_t peak;
    uint32_t nonzero;
    uint32_t use_right;
    uint32_t last_ret;
    uint32_t partial_read;
    uint32_t accum_timeout;
} OhosAudioEs7210PcmFrameSnapshot;

typedef struct {
    uint32_t task_started;
    uint32_t running;
    uint32_t stop_req;
    uint32_t ring_level;
    uint32_t ring_capacity;
    uint32_t produced_samples;
    uint32_t consumed_samples;
    uint32_t read_ok;
    uint32_t read_fail;
    uint32_t overflow;
    uint32_t underflow;
    uint32_t peak;
    uint32_t nonzero;
    uint32_t use_right;
    uint32_t last_ret;
    uint32_t last_bytes;
    uint32_t min_level;
    uint32_t max_level;
} OhosAudioEs7210UplinkCaptureSnapshot;

typedef void (*OhosAudioEs7210PcmTap)(const int16_t *pcm,
                                      uint32_t samples,
                                      void *userData);

void OhosAudioEs7210GetRecordStatsSnapshot(OhosAudioEs7210RecordStatsSnapshot *snap);
void OhosAudioEs7210GetPcmFrameSnapshot(OhosAudioEs7210PcmFrameSnapshot *snap);
void OhosAudioEs7210GetUplinkCaptureSnapshot(OhosAudioEs7210UplinkCaptureSnapshot *snap);

uint32_t OhosAudioEs7210StartRecordStatsTest(void);
uint32_t OhosAudioEs7210StartLoopbackTest(void);
uint32_t OhosAudioEs7210ReadPcm16MonoFrame(int16_t *out,
                                           uint32_t samples,
                                           uint32_t timeoutMs);
uint32_t OhosAudioEs7210StartUplinkCapture(void);
void OhosAudioEs7210StopUplinkCapture(void);
uint32_t OhosAudioEs7210IsUplinkCaptureStopping(void);
uint32_t OhosAudioEs7210ReadUplinkCaptureFrame(int16_t *out,
                                               uint32_t samples,
                                               uint32_t waitMs);
uint32_t OhosAudioEs7210GetUplinkCaptureLevel(void);
void OhosAudioEs7210SetPcmTap(OhosAudioEs7210PcmTap tap, void *userData);
void OhosAudioEs7210RequestPcmStopForDownlink(void);
void OhosAudioEs7210ClearPcmStopForDownlink(void);
uint32_t OhosAudioEs7210PrepareForDownlink(uint32_t settleMs);

#ifdef __cplusplus
}
#endif


uint32_t OhosAudioEs7210StopRecordStats(void);

uint32_t OhosAudioEs7210StopLoopbackTest(void);
#endif

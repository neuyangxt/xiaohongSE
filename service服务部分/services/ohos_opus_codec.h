#ifndef OHOS_OPUS_CODEC_H
#define OHOS_OPUS_CODEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OHOS_OPUS_SAMPLE_RATE       16000U
#define OHOS_OPUS_CHANNELS          1U
#define OHOS_OPUS_MAX_FRAME_BYTES   1500U
#define OHOS_OPUS_MAX_PCM_SAMPLES   1920U
#define OHOS_OPUS_UPLINK_FRAME_MS   40U
#define OHOS_OPUS_UPLINK_PCM_SAMPLES 640U
#define OHOS_OPUS_UPLINK_BITRATE    16000U
#define OHOS_OPUS_UPLINK_TARGET_BYTES 80U
#define OHOS_OPUS_UPLINK_OUT_CAP    120U

typedef struct {
    uint32_t valid;
    uint16_t opus_len;
    int32_t frames;
    int32_t samples_per_frame;
    int32_t samples;
    int32_t channels;
    int32_t error;
} OhosOpusPacketInfo;

typedef struct {
    uint32_t init_ok;
    uint32_t init_count;
    uint32_t decode_ok;
    uint32_t decode_fail;
    uint32_t last_opus_len;
    uint32_t last_samples;
    int32_t last_error;
    uint32_t enc_init_ok;
    uint32_t enc_init_count;
    uint32_t encode_ok;
    uint32_t encode_fail;
    uint32_t last_encode_samples;
    uint32_t last_encode_bytes;
    int32_t last_encode_error;
    uint32_t packet_check_ok;
    uint32_t packet_check_fail;
    uint32_t last_packet_len;
    int32_t last_packet_frames;
    int32_t last_packet_samples;
    int32_t last_packet_error;
} OhosOpusCodecSnapshot;

int32_t OhosOpusDecoderInit(void);
int32_t OhosOpusDecoderReset(void);
int32_t OhosOpusValidatePacket(const uint8_t *opusData,
                               uint16_t opusLen,
                               OhosOpusPacketInfo *info);
int32_t OhosOpusDecode(const uint8_t *opusData,
                       uint16_t opusLen,
                       int16_t *pcmOut,
                       uint16_t maxSamples,
                       uint16_t *outSamples);
int32_t OhosOpusEncoderInit(void);
int32_t OhosOpusEncodePcm16Mono(const int16_t *pcmIn,
                                uint16_t samples,
                                uint8_t *opusOut,
                                uint16_t outCap,
                                uint16_t *outLen);
void OhosOpusCodecGetSnapshot(OhosOpusCodecSnapshot *snap);

#ifdef __cplusplus
}
#endif

#endif

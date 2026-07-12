#include "ohos_opus_codec.h"

#include <string.h>

#include "esp_rom_sys.h"
#include "opus.h"

#define OHOS_OPUS_DEC_STORAGE_CAP  (24U * 1024U)
#define OHOS_OPUS_ENC_STORAGE_CAP  (64U * 1024U)

#if defined(__GNUC__) || defined(__clang__)
#define OHOS_OPUS_DEC_ALIGN __attribute__((aligned(16)))
#else
#define OHOS_OPUS_DEC_ALIGN
#endif

static uint8_t g_opus_dec_storage[OHOS_OPUS_DEC_STORAGE_CAP] OHOS_OPUS_DEC_ALIGN;
static uint8_t g_opus_enc_storage[OHOS_OPUS_ENC_STORAGE_CAP] OHOS_OPUS_DEC_ALIGN;
static OpusDecoder *g_opus_decoder = NULL;
static OpusEncoder *g_opus_encoder = NULL;

static volatile uint32_t g_opus_init_ok = 0;
static volatile uint32_t g_opus_init_count = 0;
static volatile uint32_t g_opus_decode_ok = 0;
static volatile uint32_t g_opus_decode_fail = 0;
static volatile uint32_t g_opus_last_opus_len = 0;
static volatile uint32_t g_opus_last_samples = 0;
static volatile int32_t g_opus_last_error = 0;
static volatile uint32_t g_opus_enc_init_ok = 0;
static volatile uint32_t g_opus_enc_init_count = 0;
static volatile uint32_t g_opus_encode_ok = 0;
static volatile uint32_t g_opus_encode_fail = 0;
static volatile uint32_t g_opus_last_encode_samples = 0;
static volatile uint32_t g_opus_last_encode_bytes = 0;
static volatile int32_t g_opus_last_encode_error = 0;
static volatile uint32_t g_opus_packet_check_ok = 0;
static volatile uint32_t g_opus_packet_check_fail = 0;
static volatile uint32_t g_opus_last_packet_len = 0;
static volatile int32_t g_opus_last_packet_frames = 0;
static volatile int32_t g_opus_last_packet_samples = 0;
static volatile int32_t g_opus_last_packet_error = 0;

int32_t OhosOpusValidatePacket(const uint8_t *opusData,
                               uint16_t opusLen,
                               OhosOpusPacketInfo *info)
{
    int frames = 0;
    int samplesPerFrame = 0;
    int samples = 0;
    int channels = 0;
    int32_t error = 0;

    if (info != NULL) {
        memset(info, 0, sizeof(*info));
        info->opus_len = opusLen;
    }

    g_opus_last_packet_len = opusLen;

    if (opusData == NULL || opusLen == 0U || opusLen > OHOS_OPUS_MAX_FRAME_BYTES) {
        error = OPUS_BAD_ARG;
        goto fail;
    }

    frames = opus_packet_get_nb_frames(opusData, (opus_int32)opusLen);
    if (frames <= 0 || frames > 48) {
        error = frames;
        goto fail;
    }

    samplesPerFrame = opus_packet_get_samples_per_frame(opusData,
                                                        (opus_int32)OHOS_OPUS_SAMPLE_RATE);
    if (samplesPerFrame <= 0 || samplesPerFrame > (int)OHOS_OPUS_MAX_PCM_SAMPLES) {
        error = OPUS_INVALID_PACKET;
        goto fail_with_packet_fields;
    }

    samples = opus_packet_get_nb_samples(opusData,
                                         (opus_int32)opusLen,
                                         (opus_int32)OHOS_OPUS_SAMPLE_RATE);
    if (samples <= 0 || samples > (int)OHOS_OPUS_MAX_PCM_SAMPLES) {
        error = samples;
        goto fail_with_packet_fields;
    }

    channels = opus_packet_get_nb_channels(opusData);
    if (channels != (int)OHOS_OPUS_CHANNELS) {
        error = OPUS_INVALID_PACKET;
        goto fail_with_packet_fields;
    }

    g_opus_packet_check_ok++;
    g_opus_last_packet_frames = frames;
    g_opus_last_packet_samples = samples;
    g_opus_last_packet_error = OPUS_OK;

    if (info != NULL) {
        info->valid = 1U;
        info->frames = frames;
        info->samples_per_frame = samplesPerFrame;
        info->samples = samples;
        info->channels = channels;
        info->error = OPUS_OK;
    }

    return 0;

fail_with_packet_fields:
    g_opus_last_packet_frames = frames;
    g_opus_last_packet_samples = 0;
    if (samplesPerFrame > 0) {
        g_opus_last_packet_samples = samplesPerFrame * ((frames > 0) ? frames : 1);
    }
    goto fail_record;

fail:
    g_opus_last_packet_frames = 0;
    g_opus_last_packet_samples = 0;

fail_record:
    g_opus_packet_check_fail++;
    g_opus_last_packet_error = error;
    if (info != NULL) {
        info->valid = 0U;
        info->frames = g_opus_last_packet_frames;
        info->samples_per_frame = samplesPerFrame;
        info->samples = g_opus_last_packet_samples;
        info->channels = 0;
        info->error = error;
    }
    if (g_opus_packet_check_fail <= 10U || (g_opus_packet_check_fail % 20U) == 0U) {
        esp_rom_printf("[OHOS-OPUS] packet invalid len=%u frames=%ld samples=%ld spf=%ld err=%ld ok=%u fail=%u\n",
                       opusLen,
                       (long)g_opus_last_packet_frames,
                       (long)g_opus_last_packet_samples,
                       (long)((info != NULL) ? info->samples_per_frame : samplesPerFrame),
                       (long)error,
                       g_opus_packet_check_ok,
                       g_opus_packet_check_fail);
    }
    return error;
}

int32_t OhosOpusDecoderInit(void)
{
    int decSize;
    int ret;

    if (g_opus_init_ok && g_opus_decoder != NULL) {
        return 0;
    }

    decSize = opus_decoder_get_size((int)OHOS_OPUS_CHANNELS);
    if (decSize <= 0 || decSize > (int)OHOS_OPUS_DEC_STORAGE_CAP) {
        g_opus_last_error = decSize;
        esp_rom_printf("[OHOS-OPUS] decoder storage invalid need=%d cap=%u\n",
                       decSize,
                       OHOS_OPUS_DEC_STORAGE_CAP);
        return -1;
    }

    memset(g_opus_dec_storage, 0, sizeof(g_opus_dec_storage));
    g_opus_decoder = (OpusDecoder *)g_opus_dec_storage;
    ret = opus_decoder_init(g_opus_decoder,
                            (opus_int32)OHOS_OPUS_SAMPLE_RATE,
                            (int)OHOS_OPUS_CHANNELS);
    g_opus_init_count++;
    g_opus_last_error = ret;
    g_opus_init_ok = (ret == OPUS_OK);

    esp_rom_printf("[OHOS-OPUS] decoder init ret=%d ok=%u sampleRate=%u channels=%u need=%d cap=%u initCnt=%u\n",
                   ret,
                   g_opus_init_ok,
                   OHOS_OPUS_SAMPLE_RATE,
                   OHOS_OPUS_CHANNELS,
                   decSize,
                   OHOS_OPUS_DEC_STORAGE_CAP,
                   g_opus_init_count);

    return (ret == OPUS_OK) ? 0 : ret;
}

int32_t OhosOpusDecoderReset(void)
{
    int ret;

    if (!g_opus_init_ok || g_opus_decoder == NULL) {
        return OhosOpusDecoderInit();
    }

    ret = opus_decoder_ctl(g_opus_decoder, OPUS_RESET_STATE);
    g_opus_last_error = ret;
    if (ret != OPUS_OK) {
        esp_rom_printf("[OHOS-OPUS] decoder reset fail ret=%d initOk=%u\n",
                       ret,
                       g_opus_init_ok);
        return ret;
    }

    esp_rom_printf("[OHOS-OPUS] decoder reset ok decodeOk=%u decodeFail=%u\n",
                   g_opus_decode_ok,
                   g_opus_decode_fail);
    return 0;
}

int32_t OhosOpusDecode(const uint8_t *opusData,
                       uint16_t opusLen,
                       int16_t *pcmOut,
                       uint16_t maxSamples,
                       uint16_t *outSamples)
{
    int samples;

    if (outSamples != NULL) {
        *outSamples = 0;
    }

    if (opusData == NULL || opusLen == 0U || pcmOut == NULL || maxSamples == 0U) {
        g_opus_decode_fail++;
        g_opus_last_error = OPUS_BAD_ARG;
        return OPUS_BAD_ARG;
    }

    if (!g_opus_init_ok || g_opus_decoder == NULL) {
        int32_t initRet = OhosOpusDecoderInit();
        if (initRet != 0) {
            g_opus_decode_fail++;
            return initRet;
        }
    }

    samples = opus_decode(g_opus_decoder,
                          opusData,
                          (opus_int32)opusLen,
                          pcmOut,
                          (int)maxSamples,
                          0);
    g_opus_last_opus_len = opusLen;
    g_opus_last_error = samples;

    if (samples < 0) {
        g_opus_decode_fail++;
        esp_rom_printf("[OHOS-OPUS] decode fail opusLen=%u ret=%d ok=%u fail=%u\n",
                       opusLen,
                       samples,
                       g_opus_decode_ok,
                       g_opus_decode_fail);
        return samples;
    }

    g_opus_decode_ok++;
    g_opus_last_samples = (uint32_t)samples;
    if (outSamples != NULL) {
        *outSamples = (uint16_t)samples;
    }

    if (g_opus_decode_ok <= 3U || (g_opus_decode_ok % 100U) == 0U) {
        esp_rom_printf("[OHOS-OPUS] decode ok opusLen=%u samples=%d decodeOk=%u decodeFail=%u\n",
                       opusLen,
                       samples,
                       g_opus_decode_ok,
                       g_opus_decode_fail);
    }

    return 0;
}

int32_t OhosOpusEncoderInit(void)
{
    int encSize;
    int ret;

    if (g_opus_enc_init_ok && g_opus_encoder != NULL) {
        return 0;
    }

    encSize = opus_encoder_get_size((int)OHOS_OPUS_CHANNELS);
    if (encSize <= 0 || encSize > (int)OHOS_OPUS_ENC_STORAGE_CAP) {
        g_opus_last_encode_error = encSize;
        esp_rom_printf("[OHOS-OPUS-ENC] encoder storage invalid need=%d cap=%u\n",
                       encSize,
                       OHOS_OPUS_ENC_STORAGE_CAP);
        return -1;
    }

    memset(g_opus_enc_storage, 0, sizeof(g_opus_enc_storage));
    g_opus_encoder = (OpusEncoder *)g_opus_enc_storage;
    ret = opus_encoder_init(g_opus_encoder,
                            (opus_int32)OHOS_OPUS_SAMPLE_RATE,
                            (int)OHOS_OPUS_CHANNELS,
                            OPUS_APPLICATION_AUDIO);

    if (ret == OPUS_OK) {
        int ctlRet = OPUS_OK;
        ctlRet = opus_encoder_ctl(g_opus_encoder, OPUS_SET_BITRATE((opus_int32)OHOS_OPUS_UPLINK_BITRATE));
        if (ctlRet == OPUS_OK) {
            ctlRet = opus_encoder_ctl(g_opus_encoder, OPUS_SET_VBR(0));
        }
        if (ctlRet == OPUS_OK) {
            ctlRet = opus_encoder_ctl(g_opus_encoder, OPUS_SET_FORCE_CHANNELS((int)OHOS_OPUS_CHANNELS));
        }
        if (ctlRet == OPUS_OK) {
            ctlRet = opus_encoder_ctl(g_opus_encoder, OPUS_SET_LSB_DEPTH(16));
        }
        if (ctlRet == OPUS_OK) {
            ctlRet = opus_encoder_ctl(g_opus_encoder, OPUS_SET_COMPLEXITY(4));
        }
        if (ctlRet != OPUS_OK) {
            ret = ctlRet;
        }
    }

    g_opus_enc_init_count++;
    g_opus_last_encode_error = ret;
    g_opus_enc_init_ok = (ret == OPUS_OK);

    esp_rom_printf("[OHOS-OPUS-ENC] init ret=%d ok=%u sampleRate=%u channels=%u frameMs=%u bitrate=%u targetBytes=%u need=%d cap=%u initCnt=%u\n",
                   ret,
                   g_opus_enc_init_ok,
                   OHOS_OPUS_SAMPLE_RATE,
                   OHOS_OPUS_CHANNELS,
                   OHOS_OPUS_UPLINK_FRAME_MS,
                   OHOS_OPUS_UPLINK_BITRATE,
                   OHOS_OPUS_UPLINK_TARGET_BYTES,
                   encSize,
                   OHOS_OPUS_ENC_STORAGE_CAP,
                   g_opus_enc_init_count);

    return (ret == OPUS_OK) ? 0 : ret;
}

int32_t OhosOpusEncodePcm16Mono(const int16_t *pcmIn,
                                uint16_t samples,
                                uint8_t *opusOut,
                                uint16_t outCap,
                                uint16_t *outLen)
{
    int bytes;

    if (outLen != NULL) {
        *outLen = 0;
    }

    if (pcmIn == NULL ||
        samples != OHOS_OPUS_UPLINK_PCM_SAMPLES ||
        opusOut == NULL ||
        outCap < OHOS_OPUS_UPLINK_TARGET_BYTES) {
        g_opus_encode_fail++;
        g_opus_last_encode_error = OPUS_BAD_ARG;
        return OPUS_BAD_ARG;
    }

    if (!g_opus_enc_init_ok || g_opus_encoder == NULL) {
        int32_t initRet = OhosOpusEncoderInit();
        if (initRet != 0) {
            g_opus_encode_fail++;
            return initRet;
        }
    }

    bytes = opus_encode(g_opus_encoder,
                        pcmIn,
                        (int)samples,
                        opusOut,
                        (opus_int32)OHOS_OPUS_UPLINK_TARGET_BYTES);
    g_opus_last_encode_samples = samples;
    g_opus_last_encode_error = bytes;

    if (bytes < 0) {
        g_opus_encode_fail++;
        esp_rom_printf("[OHOS-OPUS-ENC] encode fail samples=%u ret=%d ok=%u fail=%u\n",
                       samples,
                       bytes,
                       g_opus_encode_ok,
                       g_opus_encode_fail);
        return bytes;
    }

    g_opus_encode_ok++;
    g_opus_last_encode_bytes = (uint32_t)bytes;
    if (outLen != NULL) {
        *outLen = (uint16_t)bytes;
    }

    if ((uint32_t)bytes != OHOS_OPUS_UPLINK_TARGET_BYTES ||
        g_opus_encode_ok <= 5U ||
        (g_opus_encode_ok % 50U) == 0U) {
        esp_rom_printf("[OHOS-OPUS-ENC] encode ok samples=%u opusBytes=%d target=%u ok=%u fail=%u\n",
                       samples,
                       bytes,
                       OHOS_OPUS_UPLINK_TARGET_BYTES,
                       g_opus_encode_ok,
                       g_opus_encode_fail);
    }

    return 0;
}

void OhosOpusCodecGetSnapshot(OhosOpusCodecSnapshot *snap)
{
    if (snap == NULL) {
        return;
    }

    snap->init_ok = g_opus_init_ok;
    snap->init_count = g_opus_init_count;
    snap->decode_ok = g_opus_decode_ok;
    snap->decode_fail = g_opus_decode_fail;
    snap->last_opus_len = g_opus_last_opus_len;
    snap->last_samples = g_opus_last_samples;
    snap->last_error = g_opus_last_error;
    snap->enc_init_ok = g_opus_enc_init_ok;
    snap->enc_init_count = g_opus_enc_init_count;
    snap->encode_ok = g_opus_encode_ok;
    snap->encode_fail = g_opus_encode_fail;
    snap->last_encode_samples = g_opus_last_encode_samples;
    snap->last_encode_bytes = g_opus_last_encode_bytes;
    snap->last_encode_error = g_opus_last_encode_error;
    snap->packet_check_ok = g_opus_packet_check_ok;
    snap->packet_check_fail = g_opus_packet_check_fail;
    snap->last_packet_len = g_opus_last_packet_len;
    snap->last_packet_frames = g_opus_last_packet_frames;
    snap->last_packet_samples = g_opus_last_packet_samples;
    snap->last_packet_error = g_opus_last_packet_error;
}

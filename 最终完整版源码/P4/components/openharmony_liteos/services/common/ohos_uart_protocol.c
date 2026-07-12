#include "ohos_uart_protocol.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_rom_sys.h"
#define OHOS_UART_PROTO_LOG(...) esp_rom_printf(__VA_ARGS__)
#else
#include <stdio.h>
#define OHOS_UART_PROTO_LOG(...) printf(__VA_ARGS__)
#endif

#define OHOS_UART_HEAD_B0 0xA5U
#define OHOS_UART_HEAD_B1 0xA5U
#define OHOS_UART_HEAD_B2 0x5AU
#define OHOS_UART_HEAD_B3 0x5AU

#define OHOS_UART_TAIL_B0 0x78U
#define OHOS_UART_TAIL_B1 0x56U
#define OHOS_UART_TAIL_B2 0x34U
#define OHOS_UART_TAIL_B3 0x12U

static void PutLe16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static uint16_t GetLe16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int CheckHead(const uint8_t *p)
{
    return p[0] == OHOS_UART_HEAD_B0 &&
           p[1] == OHOS_UART_HEAD_B1 &&
           p[2] == OHOS_UART_HEAD_B2 &&
           p[3] == OHOS_UART_HEAD_B3;
}

static int CheckTail(const uint8_t *p)
{
    return p[12] == OHOS_UART_TAIL_B0 &&
           p[13] == OHOS_UART_TAIL_B1 &&
           p[14] == OHOS_UART_TAIL_B2 &&
           p[15] == OHOS_UART_TAIL_B3;
}

uint16_t OhosUartProtoCalcChecksum(const uint8_t *payload, uint16_t payloadLen)
{
    uint32_t sum = 0;

    if (payload == NULL || payloadLen == 0) {
        return 0;
    }

    for (uint16_t i = 0; i < payloadLen; i++) {
        sum += payload[i];
    }

    return (uint16_t)(sum & 0xFFFFU);
}

int OhosUartProtoEncode(uint16_t cmdType,
                        const uint8_t *payload,
                        uint16_t payloadLen,
                        uint8_t *outFrame,
                        uint16_t outCap,
                        uint16_t *outLen,
                        int enableChecksum)
{
    uint16_t totalLen;
    uint16_t checksum;

    if (outFrame == NULL || outLen == NULL) {
        return OHOS_UART_PROTO_ERR_PARAM;
    }

    if (payloadLen > OHOS_UART_PROTO_MAX_PAYLOAD_LEN) {
        return OHOS_UART_PROTO_ERR_LEN;
    }

    if (payloadLen > 0 && payload == NULL) {
        return OHOS_UART_PROTO_ERR_PARAM;
    }

    totalLen = (uint16_t)(OHOS_UART_PROTO_FRAME_FIXED_LEN + payloadLen);
    if (outCap < totalLen) {
        return OHOS_UART_PROTO_ERR_CAP;
    }

    memset(outFrame, 0, totalLen);

    outFrame[0] = OHOS_UART_HEAD_B0;
    outFrame[1] = OHOS_UART_HEAD_B1;
    outFrame[2] = OHOS_UART_HEAD_B2;
    outFrame[3] = OHOS_UART_HEAD_B3;

    checksum = enableChecksum ? OhosUartProtoCalcChecksum(payload, payloadLen) : 0;
    PutLe16(&outFrame[4], checksum);
    PutLe16(&outFrame[6], cmdType);
    PutLe16(&outFrame[8], payloadLen);
    PutLe16(&outFrame[10], OHOS_UART_PROTO_VERSION);

    outFrame[12] = OHOS_UART_TAIL_B0;
    outFrame[13] = OHOS_UART_TAIL_B1;
    outFrame[14] = OHOS_UART_TAIL_B2;
    outFrame[15] = OHOS_UART_TAIL_B3;

    if (payloadLen > 0) {
        memcpy(&outFrame[16], payload, payloadLen);
    }

    *outLen = totalLen;
    return OHOS_UART_PROTO_OK;
}

int OhosUartProtoDecode(const uint8_t *frame,
                        uint16_t frameLen,
                        OhosUartProtoFrame *out,
                        int verifyChecksum)
{
    uint16_t payloadLen;
    uint16_t expectLen;
    uint16_t checksum;

    if (frame == NULL || out == NULL) {
        return OHOS_UART_PROTO_ERR_PARAM;
    }

    if (frameLen < OHOS_UART_PROTO_FRAME_FIXED_LEN) {
        return OHOS_UART_PROTO_ERR_SHORT;
    }

    if (!CheckHead(frame)) {
        return OHOS_UART_PROTO_ERR_HEAD;
    }

    if (!CheckTail(frame)) {
        return OHOS_UART_PROTO_ERR_TAIL;
    }

    payloadLen = GetLe16(&frame[8]);
    if (payloadLen > OHOS_UART_PROTO_MAX_PAYLOAD_LEN) {
        return OHOS_UART_PROTO_ERR_LEN;
    }

    expectLen = (uint16_t)(OHOS_UART_PROTO_FRAME_FIXED_LEN + payloadLen);
    if (frameLen < expectLen) {
        return OHOS_UART_PROTO_ERR_SHORT;
    }

    checksum = GetLe16(&frame[4]);
    if (verifyChecksum) {
        uint16_t actual = OhosUartProtoCalcChecksum(&frame[16], payloadLen);
        if (checksum != actual) {
            return OHOS_UART_PROTO_ERR_CHECKSUM;
        }
    }

    out->checksum = checksum;
    out->cmd_type = GetLe16(&frame[6]);
    out->payload_len = payloadLen;
    out->version = GetLe16(&frame[10]);
    out->payload = (payloadLen > 0) ? &frame[16] : NULL;

    return OHOS_UART_PROTO_OK;
}


int OhosUartProtoFindFrame(const uint8_t *stream,
                           uint16_t streamLen,
                           uint16_t *frameOffset,
                           uint16_t *frameLen,
                           OhosUartProtoFrame *out,
                           int verifyChecksum)
{
    if (stream == NULL || frameOffset == NULL || frameLen == NULL || out == NULL) {
        return OHOS_UART_PROTO_ERR_PARAM;
    }

    if (streamLen < OHOS_UART_PROTO_FRAME_FIXED_LEN) {
        return OHOS_UART_PROTO_ERR_SHORT;
    }

    for (uint16_t i = 0; i <= (uint16_t)(streamLen - 4U); i++) {
        uint16_t remain;
        uint16_t payloadLen;
        uint16_t totalLen;

        if (!(stream[i] == OHOS_UART_HEAD_B0 &&
              stream[i + 1] == OHOS_UART_HEAD_B1 &&
              stream[i + 2] == OHOS_UART_HEAD_B2 &&
              stream[i + 3] == OHOS_UART_HEAD_B3)) {
            continue;
        }

        remain = (uint16_t)(streamLen - i);
        if (remain < OHOS_UART_PROTO_FRAME_FIXED_LEN) {
            *frameOffset = i;
            *frameLen = OHOS_UART_PROTO_FRAME_FIXED_LEN;
            return OHOS_UART_PROTO_ERR_SHORT;
        }

        if (!CheckTail(&stream[i])) {
            continue;
        }

        payloadLen = GetLe16(&stream[i + 8]);
        if (payloadLen > OHOS_UART_PROTO_MAX_PAYLOAD_LEN) {
            continue;
        }

        totalLen = (uint16_t)(OHOS_UART_PROTO_FRAME_FIXED_LEN + payloadLen);
        if (remain < totalLen) {
            *frameOffset = i;
            *frameLen = totalLen;
            return OHOS_UART_PROTO_ERR_SHORT;
        }

        *frameOffset = i;
        *frameLen = totalLen;
        return OhosUartProtoDecode(&stream[i], totalLen, out, verifyChecksum);
    }

    return OHOS_UART_PROTO_ERR_HEAD;
}

const char *OhosUartProtoCmdName(uint16_t cmdType)
{
    switch (cmdType) {
        case OHOS_UART_CMD_REQUEST_SOFT_RESET:    return "REQUEST_SOFT_RESET";
        case OHOS_UART_CMD_QUERY_SYSTEM_STATUS:   return "QUERY_SYSTEM_STATUS";
        case OHOS_UART_CMD_REPORT_SYSTEM_STATUS:  return "REPORT_SYSTEM_STATUS";
        case OHOS_UART_CMD_GET_WAKE_UP_STATUS:    return "GET_WAKE_UP_STATUS";
        case OHOS_UART_CMD_SET_WAKE_UP_STATUS:    return "SET_WAKE_UP_STATUS";
        case OHOS_UART_CMD_SET_DEVICE_ID:         return "SET_DEVICE_ID";
        case OHOS_UART_CMD_SET_P4_FW_VERSION:     return "SET_P4_FW_VERSION";
        case OHOS_UART_CMD_GET_63_FW_VERSION:     return "GET_63_FW_VERSION";
        case OHOS_UART_CMD_SET_FONT_VERSION:      return "SET_FONT_VERSION";
        case OHOS_UART_CMD_GET_AGENT_STATUS:      return "AGENT_STATUS";
        case OHOS_UART_CMD_SET_WS_SERVER_URL:     return "SET_WS_SERVER_URL";
        case OHOS_UART_CMD_SET_OTA_URL:           return "SET_OTA_URL";
        case OHOS_UART_CMD_SET_INTERRUPT_MODE:    return "SET_INTERRUPT_MODE";
        case OHOS_UART_CMD_SET_ACTIVATE_CODE:     return "SET_ACTIVATE_CODE";
        case OHOS_UART_CMD_WIFI_SEND_SSID:        return "WIFI_SEND_SSID";
        case OHOS_UART_CMD_WIFI_SEND_PSWD:        return "WIFI_SEND_PSWD";
        case OHOS_UART_CMD_WIFI_STATUS:           return "WIFI_STATUS";
        case OHOS_UART_CMD_WIFI_INIT_CONNECT:     return "WIFI_INIT_CONNECT";
        case OHOS_UART_CMD_WIFI_RE_CONNECT:       return "WIFI_RE_CONNECT";
        case OHOS_UART_CMD_WIFI_DISCONNECT:       return "WIFI_DISCONNECT";
        case OHOS_UART_CMD_WIFI_START_SCAN:       return "WIFI_START_SCAN";
//      case OHOS_UART_CMD_WIFI_SCAN_RESULT:      return "WIFI_SCAN_RESULT";
        case OHOS_UART_CMD_SET_VOICE_WAKE_UP:     return "SET_VOICE_WAKE_UP";
        case OHOS_UART_CMD_SET_VAD_START:         return "SET_VAD_START";
        case OHOS_UART_CMD_SET_FORCE_VAD_END:     return "SET_FORCE_VAD_END";
        case OHOS_UART_CMD_DOWN_AUDIO_DATA_START: return "DOWN_AUDIO_DATA_START";
        case OHOS_UART_CMD_DOWN_AUDIO_DATA_STOP:  return "DOWN_AUDIO_DATA_STOP";
        case OHOS_UART_CMD_UP_TEXT_DATA:          return "UP_TEXT_DATA";
        case OHOS_UART_CMD_UP_AUDIO_DATA:         return "UP_AUDIO_DATA";
        case OHOS_UART_CMD_UP_BIN_DATA_BEGIN:     return "UP_BIN_DATA_BEGIN";
        case OHOS_UART_CMD_DOWN_TEXT_DATA:        return "DOWN_TEXT_DATA";
        case OHOS_UART_CMD_DOWN_AUDIO_DATA:       return "DOWN_AUDIO_DATA";
        case OHOS_UART_CMD_SENSOR_TH_REPORT:      return "SENSOR_TH_REPORT";
        case OHOS_UART_CMD_SENSOR_DATA_QUERY:     return "SENSOR_DATA_QUERY";
        case OHOS_UART_CMD_SENSOR_CONTROL:        return "SENSOR_CONTROL";
        case OHOS_UART_CMD_SCENE_CONTROL:         return "SCENE_CONTROL";
        default: return "UNKNOWN";
    }
}

int OhosUartProtoSelfTest(void)
{
    uint8_t frame[OHOS_UART_PROTO_FRAME_FIXED_LEN + 4];
    uint8_t payload[1] = {0};
    uint16_t frameLen = 0;
    OhosUartProtoFrame parsed;
    int ret;
    int ok;

    ret = OhosUartProtoEncode(OHOS_UART_CMD_QUERY_SYSTEM_STATUS,
                              NULL,
                              0,
                              frame,
                              sizeof(frame),
                              &frameLen,
                              0);
    ok = (ret == OHOS_UART_PROTO_OK &&
          frameLen == OHOS_UART_PROTO_FRAME_FIXED_LEN &&
          frame[0] == 0xA5 &&
          frame[1] == 0xA5 &&
          frame[2] == 0x5A &&
          frame[3] == 0x5A &&
          frame[6] == 0x02 &&
          frame[7] == 0x0A &&
          frame[12] == 0x78 &&
          frame[13] == 0x56 &&
          frame[14] == 0x34 &&
          frame[15] == 0x12);

    OHOS_UART_PROTO_LOG("[OHOS-S47A] proto encode cmd=0x%04x len=0 frameLen=%u ret=%d ok=%d\n",
                        OHOS_UART_CMD_QUERY_SYSTEM_STATUS, frameLen, ret, ok);

    ret = OhosUartProtoDecode(frame, frameLen, &parsed, 0);
    ok = ok &&
         ret == OHOS_UART_PROTO_OK &&
         parsed.cmd_type == OHOS_UART_CMD_QUERY_SYSTEM_STATUS &&
         parsed.payload_len == 0;

    OHOS_UART_PROTO_LOG("[OHOS-S47A] proto decode cmd=0x%04x name=%s len=%u ret=%d ok=%d\n",
                        parsed.cmd_type,
                        OhosUartProtoCmdName(parsed.cmd_type),
                        parsed.payload_len,
                        ret,
                        ok);

    payload[0] = 0;
    ret = OhosUartProtoEncode(OHOS_UART_CMD_REPORT_SYSTEM_STATUS,
                              payload,
                              1,
                              frame,
                              sizeof(frame),
                              &frameLen,
                              0);
    ok = ok &&
         ret == OHOS_UART_PROTO_OK &&
         frameLen == (OHOS_UART_PROTO_FRAME_FIXED_LEN + 1) &&
         frame[6] == 0x03 &&
         frame[7] == 0x0A &&
         frame[8] == 0x01 &&
         frame[9] == 0x00 &&
         frame[16] == 0x00;

    OHOS_UART_PROTO_LOG("[OHOS-S47A] proto encode cmd=0x%04x status=%u frameLen=%u ret=%d ok=%d\n",
                        OHOS_UART_CMD_REPORT_SYSTEM_STATUS,
                        payload[0],
                        frameLen,
                        ret,
                        ok);

    ret = OhosUartProtoDecode(frame, frameLen, &parsed, 0);
    ok = ok &&
         ret == OHOS_UART_PROTO_OK &&
         parsed.cmd_type == OHOS_UART_CMD_REPORT_SYSTEM_STATUS &&
         parsed.payload_len == 1 &&
         parsed.payload != NULL &&
         parsed.payload[0] == 0;

    OHOS_UART_PROTO_LOG("[OHOS-S47A] proto decode cmd=0x%04x name=%s status=%u ret=%d finalOk=%d\n",
                        parsed.cmd_type,
                        OhosUartProtoCmdName(parsed.cmd_type),
                        parsed.payload ? parsed.payload[0] : 255,
                        ret,
                        ok);


    {
        uint8_t stream[32];
        uint16_t streamLen;
        uint16_t foundOffset = 0;
        uint16_t foundFrameLen = 0;
        OhosUartProtoFrame found;

        memset(stream, 0, sizeof(stream));
        stream[0] = 0x55;
        stream[1] = 0xAA;
        memcpy(&stream[2], frame, frameLen);
        streamLen = (uint16_t)(frameLen + 2U);

        ret = OhosUartProtoFindFrame(stream,
                                     streamLen,
                                     &foundOffset,
                                     &foundFrameLen,
                                     &found,
                                     0);

        ok = ok &&
             ret == OHOS_UART_PROTO_OK &&
             foundOffset == 2 &&
             foundFrameLen == frameLen &&
             found.cmd_type == OHOS_UART_CMD_REPORT_SYSTEM_STATUS &&
             found.payload_len == 1 &&
             found.payload != NULL &&
             found.payload[0] == 0;

        OHOS_UART_PROTO_LOG("[OHOS-S47D] proto stream find offset=%u frameLen=%u cmd=0x%04x name=%s ret=%d ok=%d\n",
                            foundOffset,
                            foundFrameLen,
                            found.cmd_type,
                            OhosUartProtoCmdName(found.cmd_type),
                            ret,
                            ok);
    }

    return ok ? OHOS_UART_PROTO_OK : OHOS_UART_PROTO_ERR_PARAM;
}

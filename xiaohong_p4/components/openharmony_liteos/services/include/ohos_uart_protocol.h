#ifndef OHOS_UART_PROTOCOL_H
#define OHOS_UART_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OHOS_UART_PROTO_FRAME_FIXED_LEN      16U
#define OHOS_UART_PROTO_VERSION             0x0000U
#define OHOS_UART_PROTO_MAX_PAYLOAD_LEN      4096U
#define OHOS_UART_PROTO_MAX_FRAME_LEN        (OHOS_UART_PROTO_FRAME_FIXED_LEN + OHOS_UART_PROTO_MAX_PAYLOAD_LEN)

#define OHOS_UART_PROTO_OK                   0
#define OHOS_UART_PROTO_ERR_PARAM           -1
#define OHOS_UART_PROTO_ERR_SHORT           -2
#define OHOS_UART_PROTO_ERR_HEAD            -3
#define OHOS_UART_PROTO_ERR_TAIL            -4
#define OHOS_UART_PROTO_ERR_LEN             -5
#define OHOS_UART_PROTO_ERR_CHECKSUM        -6
#define OHOS_UART_PROTO_ERR_CAP             -7

/*
 * Frame format from protocol table:
 *
 * byte[0..3]   package_head: A5 A5 5A 5A
 * byte[4..5]   checksum, little-endian, default 0x0000
 * byte[6..7]   cmd_type, little-endian, e.g. 0x0A03 sent as 03 0A
 * byte[8..9]   payload_len, little-endian
 * byte[10..11] version, little-endian, default 0x0000
 * byte[12..15] package_tail: 78 56 34 12
 * byte[16..]   payload
 */
// Rx: static void UartLinkHandleRxFrame(const OhosUartProtoFrame *frame)
// Tx: static uint32_t UartLinkSendProtocolFrame(uint16_t cmdType, .....)
                                  //CMD direction: P4 Rx <<==>> P4 Tx
#define OHOS_UART_CMD_REQUEST_SOFT_RESET     0x0A01U  // <<==>>
#define OHOS_UART_CMD_QUERY_SYSTEM_STATUS    0x0A02U  // <<==>>
#define OHOS_UART_CMD_REPORT_SYSTEM_STATUS   0x0A03U  // <<==>>

#define OHOS_UART_CMD_GET_WAKE_UP_STATUS     0x0B01U  //   ==>>
#define OHOS_UART_CMD_SET_WAKE_UP_STATUS     0x0B02U  // <<==>>
#define OHOS_UART_CMD_SET_DEVICE_ID          0x0B03U  // <<==>>
#define OHOS_UART_CMD_SET_P4_FW_VERSION      0x0B04U  //   ==>>
#define OHOS_UART_CMD_GET_P4_FW_VERSION      0x0B04U  // <<==
#define OHOS_UART_CMD_SET_63_FW_VERSION      0x0B05U  // <<==
#define OHOS_UART_CMD_GET_63_FW_VERSION      0x0B05U  //   ==>>
#define OHOS_UART_CMD_SET_FONT_VERSION       0x0B06U  //   ==>>
#define OHOS_UART_CMD_GET_FONT_VERSION       0x0B06U  // <<==
#define OHOS_UART_CMD_GET_AGENT_STATUS       0x0B07U  //   ==>>
#define OHOS_UART_CMD_SET_AGENT_STATUS       0x0B07U  // <<==>>
#define OHOS_UART_CMD_GET_WS_SERVER_URL      0x0B08U  //   ==>>
#define OHOS_UART_CMD_SET_WS_SERVER_URL      0x0B08U  // <<==
#define OHOS_UART_CMD_SET_OTA_URL            0x0B09U  // <<==>>
#define OHOS_UART_CMD_SET_INTERRUPT_MODE     0x0B0AU  // <<==>>
#define OHOS_UART_CMD_SET_ACTIVATE_CODE      0x0B0BU  // <<==

#define OHOS_UART_CMD_WIFI_SEND_SSID         0x0D01U  // <<==>>
#define OHOS_UART_CMD_WIFI_SEND_PSWD         0x0D02U  // <<==>>
#define OHOS_UART_CMD_WIFI_STATUS            0x0D03U  // <<==>>
#define OHOS_UART_CMD_WIFI_INIT_CONNECT      0x0D04U  //   ==>>
#define OHOS_UART_CMD_WIFI_RE_CONNECT        0x0D05U  //   ==>>
#define OHOS_UART_CMD_WIFI_DISCONNECT        0x0D06U  //   ==>>
#define OHOS_UART_CMD_WIFI_START_SCAN        0x0D07U  //   ==>>
#define OHOS_UART_CMD_WIFI_SCAN_RESULT       0x0D07U  // <<==

#define OHOS_UART_CMD_SET_VOICE_WAKE_UP      0x0E01U
#define OHOS_UART_CMD_SET_VAD_START          0x0E02U
#define OHOS_UART_CMD_SET_FORCE_VAD_END      0x0E03U
#define OHOS_UART_CMD_DOWN_AUDIO_DATA_START  0x0E04U
#define OHOS_UART_CMD_DOWN_AUDIO_DATA_STOP   0x0E05U

#define OHOS_UART_CMD_UP_TEXT_DATA           0x0E20U
#define OHOS_UART_CMD_UP_AUDIO_DATA          0x0E21U
#define OHOS_UART_CMD_UP_BIN_DATA_BEGIN      0x0E22U

#define OHOS_UART_CMD_DOWN_TEXT_DATA         0x0E40U
#define OHOS_UART_CMD_DOWN_AUDIO_DATA        0x0E41U

#define OHOS_UART_CMD_SENSOR_TH_REPORT       0x0F10U
#define OHOS_UART_CMD_SENSOR_DATA_QUERY      0x0F11U
#define OHOS_UART_CMD_SENSOR_CONTROL         0x0F12U
#define OHOS_UART_CMD_SCENE_CONTROL          0x0F13U

/* Compatibility names used by the stable manual alarm buttons. */
#define OHOS_UART_MODULE_DEVICE_ALARM        1U
#define OHOS_UART_MODULE_ACTION_OFF          0U
#define OHOS_UART_MODULE_ACTION_ON           1U
#define OHOS_UART_MODULE_ACTION_TOGGLE       2U

#define OHOS_UART_TEXT_TYPE_STT              0U
#define OHOS_UART_TEXT_TYPE_LLM              1U
#define OHOS_UART_TEXT_TYPE_TTS              2U
#define OHOS_UART_TEXT_TYPE_UP_TX            3U

typedef struct {
    uint16_t checksum;
    uint16_t cmd_type;
    uint16_t payload_len;
    uint16_t version;
    const uint8_t *payload;
} OhosUartProtoFrame;

uint16_t OhosUartProtoCalcChecksum(const uint8_t *payload, uint16_t payloadLen);

int OhosUartProtoEncode(uint16_t cmdType,
                        const uint8_t *payload,
                        uint16_t payloadLen,
                        uint8_t *outFrame,
                        uint16_t outCap,
                        uint16_t *outLen,
                        int enableChecksum);

int OhosUartProtoDecode(const uint8_t *frame,
                        uint16_t frameLen,
                        OhosUartProtoFrame *out,
                        int verifyChecksum);


int OhosUartProtoFindFrame(const uint8_t *stream,
                           uint16_t streamLen,
                           uint16_t *frameOffset,
                           uint16_t *frameLen,
                           OhosUartProtoFrame *out,
                           int verifyChecksum);

const char *OhosUartProtoCmdName(uint16_t cmdType);

int OhosUartProtoSelfTest(void);

#ifdef __cplusplus
}
#endif

#endif

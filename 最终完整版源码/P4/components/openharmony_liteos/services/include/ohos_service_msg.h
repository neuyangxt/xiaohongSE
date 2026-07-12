#ifndef OHOS_SERVICE_MSG_H
#define OHOS_SERVICE_MSG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DNESP32P4 OpenHarmony LiteOS-M service message definition.
 *
 * Message ID layout:
 *   DemoService      3600 - 3699
 *   LEDService       3700 - 3799
 *   KeyService       3800 - 3899
 *   UartLinkService  3900 - 3999
 *   AudioService     4000 - 4099
 *   DisplayService   4100 - 4199
 *   CameraService    4200 - 4299
 */

#define OHOS_SERVICE_MSG_BASE_DEMO        3600
#define OHOS_SERVICE_MSG_BASE_LED         3700
#define OHOS_SERVICE_MSG_BASE_KEY         3800
#define OHOS_SERVICE_MSG_BASE_UART_LINK   3900
#define OHOS_SERVICE_MSG_BASE_AUDIO       4000
#define OHOS_SERVICE_MSG_BASE_DISPLAY     4100
#define OHOS_SERVICE_MSG_BASE_CAMERA      4200

/* DemoService */
#define OHOS_SERVICE_MSG_DEMO_PING        (OHOS_SERVICE_MSG_BASE_DEMO + 1)

/* LEDService */
#define OHOS_SERVICE_MSG_LED_CONTROL      (OHOS_SERVICE_MSG_BASE_LED + 1)
#define OHOS_SERVICE_MSG_LED_GET_STATUS   (OHOS_SERVICE_MSG_BASE_LED + 2)

/* KeyService */
#define OHOS_SERVICE_MSG_KEY_GET_STATE    (OHOS_SERVICE_MSG_BASE_KEY + 1)
#define OHOS_SERVICE_MSG_KEY_EVENT        (OHOS_SERVICE_MSG_BASE_KEY + 2)
#define OHOS_SERVICE_MSG_KEY_SELF_TEST    (OHOS_SERVICE_MSG_BASE_KEY + 3)

/* UartLinkService / WS63LinkService */
#define OHOS_SERVICE_MSG_UART_SEND        (OHOS_SERVICE_MSG_BASE_UART_LINK + 1)
#define OHOS_SERVICE_MSG_UART_RECV        (OHOS_SERVICE_MSG_BASE_UART_LINK + 2)
#define OHOS_SERVICE_MSG_UART_GET_STATUS  (OHOS_SERVICE_MSG_BASE_UART_LINK + 3)
#define OHOS_SERVICE_MSG_UART_SELF_TEST   (OHOS_SERVICE_MSG_BASE_UART_LINK + 4)



/* AudioService */
#define OHOS_SERVICE_MSG_AUDIO_GET_STATUS   (OHOS_SERVICE_MSG_BASE_AUDIO + 1)
#define OHOS_SERVICE_MSG_AUDIO_SELF_TEST    (OHOS_SERVICE_MSG_BASE_AUDIO + 2)
#define OHOS_SERVICE_MSG_AUDIO_PLAY_TEST    (OHOS_SERVICE_MSG_BASE_AUDIO + 3)
#define OHOS_SERVICE_MSG_AUDIO_RECORD_TEST  (OHOS_SERVICE_MSG_BASE_AUDIO + 4)

/* DisplayService */
#define OHOS_SERVICE_MSG_DISPLAY_GET_STATUS  (OHOS_SERVICE_MSG_BASE_DISPLAY + 1)
#define OHOS_SERVICE_MSG_DISPLAY_SELF_TEST   (OHOS_SERVICE_MSG_BASE_DISPLAY + 2)
#define OHOS_SERVICE_MSG_DISPLAY_DRAW_TEST   (OHOS_SERVICE_MSG_BASE_DISPLAY + 3)


/* CameraService */
#define OHOS_SERVICE_MSG_CAMERA_GET_STATUS   (OHOS_SERVICE_MSG_BASE_CAMERA + 1)
#define OHOS_SERVICE_MSG_CAMERA_SELF_TEST    (OHOS_SERVICE_MSG_BASE_CAMERA + 2)
#define OHOS_SERVICE_MSG_CAMERA_CAPTURE_TEST (OHOS_SERVICE_MSG_BASE_CAMERA + 3)
#define OHOS_SERVICE_MSG_CAMERA_STREAM_TEST  (OHOS_SERVICE_MSG_BASE_CAMERA + 4)

/* Generic command values */
#define OHOS_SERVICE_CMD_OFF              0U
#define OHOS_SERVICE_CMD_ON               1U
#define OHOS_SERVICE_CMD_TOGGLE           2U
#define OHOS_SERVICE_CMD_START            3U
#define OHOS_SERVICE_CMD_STOP             4U
#define OHOS_SERVICE_CMD_GET_STATUS       5U
#define OHOS_SERVICE_CMD_SELF_TEST        6U

/* Generic result values */
#define OHOS_SERVICE_RESULT_OK            0U
#define OHOS_SERVICE_RESULT_ERROR         1U
#define OHOS_SERVICE_RESULT_UNSUPPORTED   2U
#define OHOS_SERVICE_RESULT_TIMEOUT       3U

typedef struct {
    uint32_t cmd;
    uint32_t value;
    uint32_t flags;
    uint32_t reserved;
} OhosServiceRequestData;

typedef struct {
    uint32_t result;
    uint32_t value;
    uint32_t flags;
    uint32_t reserved;
} OhosServiceResponseData;

#ifdef __cplusplus
}
#endif

#endif

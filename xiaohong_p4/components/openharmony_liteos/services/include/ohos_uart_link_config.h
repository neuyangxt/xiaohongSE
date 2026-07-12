#ifndef OHOS_UART_LINK_CONFIG_H
#define OHOS_UART_LINK_CONFIG_H

#include "driver/uart.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UartLinkService hardware configuration.
 *
 * Current S40C default mode:
 *   UART1 TX -> GPIO10
 *   UART1 RX -> GPIO11
 *
 * Test method:
 *   Connect GPIO10 and GPIO11 with a jumper wire.
 *
 * Future WS63 mode:
 *   Replace OHOS_UART_LINK_TX_GPIO and OHOS_UART_LINK_RX_GPIO
 *   with the actual ESP32-P4 <-> WS63 connection pins.
 */

#ifndef OHOS_UART_LINK_PORT
#define OHOS_UART_LINK_PORT              UART_NUM_1
#endif

#ifndef OHOS_UART_LINK_TX_GPIO
#define OHOS_UART_LINK_TX_GPIO           GPIO_NUM_30
#endif

#ifndef OHOS_UART_LINK_RX_GPIO
#define OHOS_UART_LINK_RX_GPIO           GPIO_NUM_31
#endif

#ifndef OHOS_UART_LINK_BAUDRATE
#define OHOS_UART_LINK_BAUDRATE          921600
#endif

#ifndef OHOS_UART_LINK_RX_BUF_SIZE
#define OHOS_UART_LINK_RX_BUF_SIZE       8192
#endif

#ifndef OHOS_UART_LINK_TX_BUF_SIZE
#define OHOS_UART_LINK_TX_BUF_SIZE       8192
#endif

#ifdef __cplusplus
}
#endif


/*
 * S47G:
 * WS63 <-> ESP32-P4 UART protocol default parameters.
 * Protocol table: 921600, 8N1, no flow control.
 *
 * Keep protocol loopback enabled for local GPIO10-GPIO11 validation.
 * When connecting real WS63, set OHOS_UART_LINK_ENABLE_PROTOCOL_LOOPBACK_SELFTEST to 0.
 */
#ifndef OHOS_UART_LINK_PROTOCOL_BAUDRATE
#define OHOS_UART_LINK_PROTOCOL_BAUDRATE 921600
#endif

#ifdef OHOS_UART_LINK_BAUDRATE
#undef OHOS_UART_LINK_BAUDRATE
#endif
#define OHOS_UART_LINK_BAUDRATE          OHOS_UART_LINK_PROTOCOL_BAUDRATE

#ifndef OHOS_UART_LINK_ENABLE_PROTOCOL_LOOPBACK_SELFTEST
#define OHOS_UART_LINK_ENABLE_PROTOCOL_LOOPBACK_SELFTEST 0
#endif


/*
 * S47I link mode switch:
 *
 * 0: local protocol loopback mode.
 *    Keep GPIO10-GPIO11 shorted and verify TX -> RX locally.
 *
 * 1: real WS63 link mode.
 *    Connect ESP32-P4 TX/RX/GND to WS63 RX/TX/GND.
 *    Local protocol loopback is not required by selftest.
 */
#ifndef OHOS_UART_LINK_ENABLE_REAL_WS63_MODE
#define OHOS_UART_LINK_ENABLE_REAL_WS63_MODE 1
#endif

/*
 * Feed captured WS63 business frames into the same streaming parser in
 * 31-byte chunks. This validates fragmented long-frame handling before live
 * traffic arrives. It does not transmit anything on UART.
 */
#ifndef OHOS_UART_LINK_ENABLE_LONG_FRAME_SELFTEST
#define OHOS_UART_LINK_ENABLE_LONG_FRAME_SELFTEST 0
#endif
#endif

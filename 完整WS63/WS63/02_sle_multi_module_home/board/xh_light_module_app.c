#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "soc_osal.h"

#include "light_key.h"
#include "ws2812.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"

#define XH_LIGHT_STACK_SIZE 0x2800
#define XH_LIGHT_PRIO 25

#ifndef XH_LIGHT_HEARTBEAT_MS
#define XH_LIGHT_HEARTBEAT_MS 10000
#endif

#ifndef XH_LIGHT_KEY_ENABLE_DELAY_MS
#define XH_LIGHT_KEY_ENABLE_DELAY_MS 2500
#endif

/* ---- Light modes (档位) ----
 * 0 = off  关
 * 1 = warm 暖光  (一档)
 * 2 = cold 冷光  (二档)
 * 3 = dim  暗光  (三档)
 */
#define XH_LIGHT_MODE_OFF  0U
#define XH_LIGHT_MODE_WARM 1U
#define XH_LIGHT_MODE_COLD 2U
#define XH_LIGHT_MODE_DIM  3U

/* Color profiles for each mode: r, g, b, brightness */
#ifndef XH_LIGHT_WARM_R
#define XH_LIGHT_WARM_R 255
#endif
#ifndef XH_LIGHT_WARM_G
#define XH_LIGHT_WARM_G 180
#endif
#ifndef XH_LIGHT_WARM_B
#define XH_LIGHT_WARM_B 80
#endif
#ifndef XH_LIGHT_WARM_BRIGHT
#define XH_LIGHT_WARM_BRIGHT 180
#endif

#ifndef XH_LIGHT_COLD_R
#define XH_LIGHT_COLD_R 255
#endif
#ifndef XH_LIGHT_COLD_G
#define XH_LIGHT_COLD_G 255
#endif
#ifndef XH_LIGHT_COLD_B
#define XH_LIGHT_COLD_B 255
#endif
#ifndef XH_LIGHT_COLD_BRIGHT
#define XH_LIGHT_COLD_BRIGHT 200
#endif

#ifndef XH_LIGHT_DIM_R
#define XH_LIGHT_DIM_R 255
#endif
#ifndef XH_LIGHT_DIM_G
#define XH_LIGHT_DIM_G 100
#endif
#ifndef XH_LIGHT_DIM_B
#define XH_LIGHT_DIM_B 30
#endif
#ifndef XH_LIGHT_DIM_BRIGHT
#define XH_LIGHT_DIM_BRIGHT 20
#endif

static bool g_light_report_pending;
static uint8_t g_light_mode;

static void xh_light_apply_mode(uint8_t mode)
{
    int ret;
    switch (mode) {
    case XH_LIGHT_MODE_WARM:
        ret = ws2812_set_color_control(XH_LIGHT_WARM_R, XH_LIGHT_WARM_G,
                                       XH_LIGHT_WARM_B, XH_LIGHT_WARM_BRIGHT);
        break;
    case XH_LIGHT_MODE_COLD:
        ret = ws2812_set_color_control(XH_LIGHT_COLD_R, XH_LIGHT_COLD_G,
                                       XH_LIGHT_COLD_B, XH_LIGHT_COLD_BRIGHT);
        break;
    case XH_LIGHT_MODE_DIM:
        ret = ws2812_set_color_control(XH_LIGHT_DIM_R, XH_LIGHT_DIM_G,
                                       XH_LIGHT_DIM_B, XH_LIGHT_DIM_BRIGHT);
        break;
    default:
        ret = ws2812_off_control();
        mode = XH_LIGHT_MODE_OFF;
        break;
    }
    g_light_mode = mode;
    printf("[WS63-LIGHT] apply mode=%u ret=%d\r\n", mode, ret);
}

/* Cycle through modes: off -> 1(warm) -> 2(cold) -> 3(dim) -> off */
static void xh_light_cycle_mode(void)
{
    uint8_t next = g_light_mode + 1U;
    if (next > XH_LIGHT_MODE_DIM) {
        next = XH_LIGHT_MODE_OFF;
    }
    printf("[WS63-LIGHT] cycle %u->%u\r\n", g_light_mode, next);
    xh_light_apply_mode(next);
}

static void xh_light_report(void)
{
    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    ws2812_color_t color = ws2812_get_color();
    uint8_t sw = color.on ? 1 : 0;

    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_LIGHT, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ONLINE, 1) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_SWITCH, sw) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_LIGHT_MODE, g_light_mode) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_GPIO_OUTPUT, sw) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        printf("[WS63-LIGHT] report on=%u mode=%u seq=%u\r\n",
            sw, g_light_mode, (unsigned int)seq);
        (void)xh_sle_module_server_report(frame, len);
    }
}

static void xh_light_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    (void)conn_id;
    xh_proto_msg_t msg = {0};

    if (!xh_proto_decode(data, len, &msg) || msg.msg != XH_PROTO_MSG_CONTROL ||
        msg.module_id != XH_MODULE_ID_LIGHT) {
        (void)xh_sle_module_server_ack(0, XH_MODULE_ID_LIGHT, XH_ACK_INVALID);
        return;
    }

    uint8_t sw = 0;
    uint8_t mode = 0;
    if (!xh_proto_get_u8(&msg, XH_TLV_SWITCH, &sw)) {
        (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_LIGHT, XH_ACK_INVALID);
        return;
    }
    if (!xh_proto_get_u8(&msg, XH_TLV_LIGHT_MODE, &mode)) {
        mode = sw ? XH_LIGHT_MODE_WARM : XH_LIGHT_MODE_OFF;
    }
    if (mode > XH_LIGHT_MODE_DIM) {
        mode = sw ? XH_LIGHT_MODE_WARM : XH_LIGHT_MODE_OFF;
    }

    printf("[WS63-LIGHT] rx ctrl seq=%u mode=%u\r\n",
        (unsigned int)msg.seq, mode);
    xh_light_apply_mode(mode);
    xh_light_report();
    (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_LIGHT, XH_ACK_OK);
    g_light_report_pending = false;
}

static void xh_light_task(void)
{
    printf("[WS63-LIGHT] boot build=%s %s module=7 warm/cold/dim key-enabled\r\n",
        __DATE__, __TIME__);

    int init_ret = ws2812_init();
    printf("[WS63-LIGHT] ws2812 init result=%d\r\n", init_ret);
    /* No boot self-test: start with light off, wait for scene rule or key. */
    xh_light_apply_mode(XH_LIGHT_MODE_OFF);

    errcode_t server_ret = xh_sle_module_server_init(XH_MODULE_ID_LIGHT, xh_light_control_cb);
    printf("[WS63-LIGHT] server init ret=0x%X\r\n", (unsigned int)server_ret);
    xh_light_report();

    /* Delay key init to avoid spurious press during boot power settle. */
    osal_msleep(XH_LIGHT_KEY_ENABLE_DELAY_MS);
    light_key_init();
    printf("[WS63-LIGHT] key init done, ready for manual cycling\r\n");

    uint32_t since_report_ms = 0;
    while (1) {
        if (light_key_consume_pressed()) {
            xh_light_cycle_mode();
            xh_light_report();
        }

        if (g_light_report_pending || since_report_ms >= XH_LIGHT_HEARTBEAT_MS) {
            xh_light_report();
            g_light_report_pending = false;
            since_report_ms = 0;
        }
        osal_msleep(50);
        since_report_ms += 50U;
    }
}

void xh_light_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_light";
    attr.stack_size = XH_LIGHT_STACK_SIZE;
    attr.priority = XH_LIGHT_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_light_task, NULL, &attr) == NULL) {
        printf("[xh_light_module] create task fail\r\n");
        return;
    }
    printf("[xh_light_module] create task ok\r\n");
}

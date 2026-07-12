/*
 * xh_uart_p4.c
 *
 * 原本独立 init + 轮询读 UART1，与原有 63RxTask 中断接收链路冲突
 * （两个任务抢读同一 bus，导致 frame buf overflow + 丢帧）。
 *
 * 现改为通过 weak hook 挂到原有 uart_rx_data_parser 的命令分发链路：
 *   uart_rx_data_parser.c::process_ctrl_command() 的 default 分支
 *   调用 weak uart_rx_on_unknown_cmd()，本文件覆盖它，
 *   把 0x0F11/0x0F12/0x0F13 分发给 xh_sle_hub.c 的 handler。
 *
 * 不再 uapi_uart_init / uapi_uart_read，彻底避免与 63RxTask 抢数据。
 * TX 侧早已走原有 set_pending_sensor_frame() → 63TxTask 路径，无需改动。
 */

#include "xh_uart_p4.h"
#include "xh_sle_hub.h"

#include <stdio.h>
#include <stdint.h>

/* ------------------------------------------------------------------------- */
/* Weak hook 覆盖：挂到原有 UART 接收链路                                     */
/* ------------------------------------------------------------------------- */
void uart_rx_on_unknown_cmd(uint16_t cmd, const uint8_t *payload, uint32_t payload_len)
{
    if (payload == NULL && payload_len > 0) {
        return;
    }

    switch (cmd) {
    case 0x0F11:  /* SENSOR_DATA_QUERY */
        printf("[XH-UART-P4] SENSOR_DATA_QUERY len=%u\r\n", (unsigned)payload_len);
        xh_sensor_uart_handle_query(payload, payload_len);
        break;

    case 0x0F12:  /* SENSOR_CONTROL */
        printf("[XH-UART-P4] SENSOR_CONTROL len=%u\r\n", (unsigned)payload_len);
        xh_sensor_uart_handle_control(payload, payload_len);
        break;

    case 0x0F13:  /* SCENE_CONTROL */
        printf("[XH-UART-P4] SCENE_CONTROL len=%u\r\n", (unsigned)payload_len);
        xh_sensor_uart_handle_scene(payload, payload_len);
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* 启动接口（空壳，保留给 xh_module_app.c 调用）                               */
/* ------------------------------------------------------------------------- */
void xh_uart_p4_start(void)
{
    printf("[XH-UART-P4] hooked into 63RxTask (no independent UART read)\r\n");
}

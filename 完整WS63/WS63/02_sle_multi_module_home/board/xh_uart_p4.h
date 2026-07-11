/*
 * xh_uart_p4.h
 *
 * P4 ↔ WS63 Hub UART 通信层头文件。
 * 修正后不再独立读 UART，改为通过 weak hook 挂到原有 63RxTask 解析链路。
 */

#ifndef XH_UART_P4_H
#define XH_UART_P4_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 启动 P4 命令接收（空壳，实际由 63RxTask 的 weak hook 处理）。
 */
void xh_uart_p4_start(void);

#ifdef __cplusplus
}
#endif

#endif /* XH_UART_P4_H */

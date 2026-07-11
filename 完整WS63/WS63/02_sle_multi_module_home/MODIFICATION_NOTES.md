# P4 界面按键控制 WS63 Server 模组 - 代码修改说明

## 修改日期
2026-07-03

## 问题描述
P4（ESP32-P4）端有传感器控制 UI 页面，包含风扇、照明、场景控制的按钮，但 WS63 Hub 端缺少 UART 接收代码，导致 P4 发送的按键命令无法被 Hub 接收和转发到对应的 SLE Server 模组。

## 解决方案
添加 WS63 Hub 端的 UART 接收代码，实现：
1. 接收 P4 发送的 OHOS UART 协议帧
2. 解析协议帧，提取 XH TLV 格式 payload
3. 根据命令类型调用对应的 handler 函数
4. Handler 函数通过 SLE Client 发送控制命令到对应的 Server 模组

## 修改的文件

### 1. 新增文件

#### `board/xh_uart_p4.c`
UART 通信层实现，包含：
- UART 初始化（`xh_uart_p4_init()`）
- UART 接收任务（`xh_uart_p4_task()`）
- OHOS UART 协议帧解析（`xh_uart_find_frame()`）
- OHOS UART 协议编码（`xh_ohos_uart_encode()`）
- 帧处理分发（`xh_uart_p4_handle_frame()`）
- P4 命令发送接口（`xh_uart_p4_send()`）
- Hub 到 P4 的数据发送（`set_pending_sensor_frame()`, `set_pending_sensor_th_data()`）

#### `board/xh_uart_p4.h`
UART 通信层头文件，声明：
- `xh_uart_p4_start()` - 启动 UART 接收任务
- `xh_uart_p4_send()` - 发送数据给 P4

#### `board/uart_tx_data_down.h`
UART 发送接口头文件（供 `xh_sle_hub.c` 调用），声明：
- `set_pending_sensor_frame()` - 发送传感器数据帧给 P4
- `set_pending_sensor_th_data()` - 发送温湿度数据给 P4（兼容旧格式）

### 2. 修改的文件

#### `board/xh_module_app.c`
- 添加 `#include "xh_uart_p4.h"`
- 在 hub 模式初始化中添加 `xh_uart_p4_start()` 调用

#### `BUILD.gn`
- 在 hub 构建目标的 `sources` 中添加 `"board/xh_uart_p4.c"`

## 协议栈

### P4 → WS63 Hub (UART)
```
OHOS UART 协议帧格式:
┌─────────────────┬──────────┬──────────────────────────────┐
│ 字段            │ 长度     │ 说明                         │
├─────────────────┼──────────┼──────────────────────────────┤
│ package_head    │ 4 字节   │ A5 A5 5A 5A                 │
│ checksum        │ 2 字节   │ payload 所有字节求和 (LE)    │
│ cmd_type        │ 2 字节   │ 命令类型 (LE)                │
│ payload_len     │ 2 字节   │ payload 长度 (LE)            │
│ version         │ 2 字节   │ 版本 (LE), 默认 0x0000      │
│ package_tail    │ 4 字节   │ 78 56 34 12                 │
│ payload         │ 变长     │ XH TLV 格式数据              │
└─────────────────┴──────────┴──────────────────────────────┘

命令类型:
- 0x0F11: SENSOR_DATA_QUERY (查询传感器数据)
- 0x0F12: SENSOR_CONTROL (控制传感器/模组)
- 0x0F13: SCENE_CONTROL (场景控制)
```

### WS63 Hub → SLE Server (星闪)
```
XH TLV 协议帧格式 (嵌在 OHOS UART payload 中):
┌─────────────────┬──────────┬──────────────────────────────┐
│ 字段            │ 长度     │ 说明                         │
├─────────────────┼──────────┼──────────────────────────────┤
│ magic           │ 2 字节   │ 'X' 'H' (0x58 0x48)        │
│ version         │ 1 字节   │ 协议版本 (1)                 │
│ seq             │ 1 字节   │ 序列号                       │
│ module_id       │ 1 字节   │ 模组 ID                      │
│ msg             │ 1 字节   │ 消息类型                     │
│ tlv_len         │ 1 字节   │ TLV 数据长度                 │
│ tlv             │ 变长     │ TLV 数据                     │
└─────────────────┴──────────┴──────────────────────────────┘

模组 ID:
- 0x00: HUB_SCENE (场景控制)
- 0x01: SHT30 (温湿度传感器)
- 0x03: FAN (风扇)
- 0x04: BH1750 (光照传感器)
- 0x05: LD2401 (人体存在传感器)
- 0x07: LIGHT (智能照明)

消息类型:
- 0x01: HELLO (上线打招呼)
- 0x02: REPORT (数据上报)
- 0x03: CONTROL (控制命令)
- 0x04: ACK (应答)
- 0x05: HEARTBEAT (心跳/查询)
```

## 代码流程

### P4 按键 → 模组控制 完整流程
```
P4 触摸屏按钮
    ↓
LVGL 事件回调 (p4_sensor_page.c)
    ↓
OhosUartLinkUiSetFanLevel() / OhosUartLinkUiSetLightLevel() / OhosUartLinkUiSetSceneMode()
    ↓
P4XhTlvBuildFanLevelControl() 构建 XH TLV 帧
    ↓
UartLinkSendProtocolFrame() 封装 OHOS UART 协议帧
    ↓
UART TX (GPIO30/31, 921600 baud)
    ↓
WS63 Hub UART RX
    ↓
xh_uart_p4_task() 接收并解析 OHOS UART 帧
    ↓
xh_uart_p4_handle_frame() 根据 cmd_type 分发
    ↓
xh_sensor_uart_handle_control() / xh_sensor_uart_handle_scene()
    ↓
xh_sle_hub_send_control() 通过 SLE Client 发送控制命令
    ↓
ssapc_write_req() SLE 写请求
    ↓
SLE Server 模组接收
    ↓
ssaps_write_request_cbk() 写请求回调
    ↓
xh_*_control_cb() 模组控制回调 (风扇/照明)
    ↓
GPIO/外设控制 (fan_gpio_set_level() / ws2812_set_color_control())
    ↓
模组状态上报 (NOTIFY)
    ↓
Hub 接收上报，转发给 P4
    ↓
P4 UI 更新
```

## 编译和测试

### 1. 编译 WS63 Hub 固件
```bash
cd /home/ccl/xiaohongSE/vendor/atomgit/xiaohong-se/examples/02_sle_multi_module_home
python3 tools/select_target.py hub
# 使用 WS63 SDK 编译命令进行编译
```

### 2. 编译 P4 固件
P4 端代码无需修改，使用原有代码即可。

### 3. 硬件连接
```
P4 (ESP32-P4)          WS63 Hub
GPIO30 (UART1 TX)  ←→ GPIO7 (UART1 RX)
GPIO31 (UART1 RX)  ←→ GPIO8 (UART1 TX)
GND                 ←→ GND
```

### 4. 测试步骤
1. 烧录 WS63 Hub 固件（编译目标: hub）
2. 烧录 WS63 模组固件（风扇: `python3 tools/select_target.py fan`，照明: `python3 tools/select_target.py light`）
3. 烧录 P4 固件
4. 上电，等待 Hub 连接各模组（通过星闪）
5. 在 P4 屏幕上进入"Device"页面
6. 点击"Control"标签页
7. 点击风扇或照明按钮，观察模组是否响应

## 已知问题和注意事项

### 1. UART 驱动 API
代码中使用了以下 UART 驱动 API：
- `uapi_uart_init()`
- `uapi_uart_read()`
- `uapi_uart_write()`

这些 API 基于 WS63 SDK 的标准 UART 驱动。如果你的 SDK 版本 API 不同，需要相应修改。

### 2. UART 接收方式
当前使用轮询方式（`uapi_uart_read()` 在任务循环中定期调用），延迟约 10ms。
- 优点: 简单，不依赖回调 API
- 缺点: 实时性稍差

如果需要更好的实时性，可以改为中断或 DMA 方式。

### 3. 协议兼容性
- P4 端使用 `p4_sensor_xh_tlv.c` 构建 XH TLV 帧
- WS63 Hub 端使用 `xh_sle_proto.c` 解析 XH TLV 帧
- 两个协议的格式必须完全匹配（当前代码已匹配）

### 4. 帧解析健壮性
当前帧解析器假设帧头和数据在一次 UART 读取中完整接收。如果 UART 数据分多次到达，可能需要多次读取才能组装完整帧。当前代码通过帧缓冲区处理了这种情况。

## 代码结构

```
02_sle_multi_module_home/
├── board/
│   ├── xh_uart_p4.c          [新增] UART 接收和发送
│   ├── xh_uart_p4.h          [新增] UART 接口头文件
│   ├── uart_tx_data_down.h   [新增] 发送接口头文件
│   ├── xh_sle_hub.c          [原有] Hub 逻辑，调用 uart_tx_data_down.h
│   ├── xh_module_app.c       [修改] 添加 xh_uart_p4_start() 调用
│   └── ...
├── proto/
│   └── xh_sle_proto.c        [原有] XH TLV 协议编解码
├── sle/
│   ├── xh_sle_client.c       [原有] SLE Client 实现
│   └── ...
└── BUILD.gn                  [修改] 添加 xh_uart_p4.c 到构建
```

## 下一步工作

1. **测试 UART 通信**: 使用逻辑分析仪或串口调试工具确认 P4 发送的 UART 数据格式正确
2. **调试帧解析**: 如果 Hub 无法接收命令，添加日志确认帧解析是否正确
3. **优化接收方式**: 如果需要更低延迟，实现 UART 中断接收
4. **添加更多模组支持**: 当前支持风扇和照明，可以添加报警器、光照传感器等

## 联系人
如有问题，请检查：
1. UART 连线是否正确（TX-RX 交叉连接）
2. 波特率是否匹配（921600）
3. 帧格式是否符合协议定义
4. WS63 Hub 是否成功连接了 SLE Server 模组

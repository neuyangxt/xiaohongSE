# 小鸿SE四模组智能家居示例

本示例包含5个固件：

- 小鸿SE开发板WS63：星闪Client/Hub，与P4通过UART通信。
- 温湿度模组：星闪Server，实时上报温湿度。
- 智能风扇：星闪Server，支持开关。
- 声光报警器：星闪Server，支持常亮、闪烁、蜂鸣和组合报警。
- 智能照明：星闪Server，支持开关、颜色和亮度。

## 场景规则

**居家模式**

- 打开照明，关闭报警器。
- 温度达到32℃或湿度高于50%时打开风扇。
- 温度降到31℃且湿度低于48%后关闭风扇。

**离家模式**

- 关闭照明和风扇。
- 温度达到35℃或湿度高于60%时启动声光报警。
- 温度降到34℃且湿度低于58%后解除报警。

温湿度在两种模式下都会持续上报。

## 选择固件

```bash
python3 vendor/atomgit/xiaohong-se/examples/02_sle_multi_module_home/tools/select_target.py hub
hb build -f
```

可选目标：

```text
hub      小鸿SE开发板
sht30    温湿度模组
fan      智能风扇
alarm    声光报警器
light    智能照明
```

## 默认引脚

```text
SHT30：SCL GPIO16，SDA GPIO15
风扇：GPIO9，按键GPIO5
报警灯：GPIO9，蜂鸣器GPIO7
WS2812：DIN GPIO9
```

引脚和阈值可以在`BUILD.gn`中修改。

import router from '@ohos.router';
import { SensorData, ControlType, FanSpeed, LightMode, SceneMode, DeviceType } from '../model/DeviceModel';
import { AppColors, AppRadius, AppSpacing, AppFontSize } from '../common/Theme';
import { sleHelper, formatTemp, formatHumi } from '../sle/helper';
import { ssap } from '@kit.NearLinkKit';
import { bytesToHex } from '../sle/XhProto';

/** 路由参数接口 */
interface DeviceRouteParams {
  name: string;
  mac: string;
  type: string;
  description: string;
}

/**
 * 设备控制页面
 * 对应 PPT: pages/Device.ets — 连接后温湿度 + SSAP 调试
 *
 * 功能：
 *   1. 显示实时温湿度（Notify）
 *   2. 控制面板：风扇、灯光、场景
 *   3. 对应 PPT 中控制风扇、灯光等模组
 */
@Entry
@Component
struct Device {
  @State deviceName: string = 'XH_HOME';
  @State deviceMac: string = '';
  @State deviceType: string = DeviceType.SENSOR;
  @State deviceDesc: string = '温湿度模组';
  @State connected: boolean = false;
  @State connecting: boolean = true;

  // 传感器数据
  @State temperatureText: string = '--.--';
  @State humidityText: string = '--.--';
  @State presenceText: string = '人在';
  @State presenceValue: number = 1;
  @State tempTrend: string = ''; // ↑ ↓ —
  @State humiTrend: string = '';

  // 控制状态
  @State fanOn: boolean = false;
  @State fanSpeed: FanSpeed = FanSpeed.MEDIUM;
  @State lightOn: boolean = false;
  @State lightMode: LightMode = LightMode.WARM;
  @State sceneMode: SceneMode = SceneMode.HOME;

  // SSAP 调试信息（对应 PPT: SSAP 调试）
  @State showDebug: boolean = false;
  @State rawDataHex: string = '暂无数据';
  @State debugLog: string = '';
  @State serviceList: ssap.Service[] = [];

  // 上一次的值，用于趋势判断
  private lastTemp: number = 0;
  private lastHumi: number = 0;
  private readonly fanTempThreshold: number = 3000;
  private readonly fanHumiThreshold: number = 7000;

  aboutToAppear(): void {
    // 接收路由参数
    const params = router.getParams() as DeviceRouteParams;
    if (params) {
      this.deviceName = params.name ?? 'XH_HOME';
      this.deviceMac = params.mac ?? '';
      this.deviceType = params.type ?? DeviceType.SENSOR;
      this.deviceDesc = params.description ?? '温湿度模组';
    }

    // 没有扫描到设备时，允许进入空数据预览界面
    if (this.deviceMac.length === 0) {
      this.connecting = false;
      this.connected = false;
      this.appendLog('当前为无设备预览模式，暂无实时数据');
      return;
    }

    // 连接设备
    this.connectDevice();
  }

  aboutToDisappear(): void {
    sleHelper.stopSensorSubscription();
    sleHelper.disconnect();
  }

  private connectDevice(): void {
    this.connecting = true;
    this.connected = false;
    this.appendLog(`正在连接 ${this.deviceName} (${this.deviceMac})`);

    // 设置原始数据回调（用于 SSAP 调试显示）
    sleHelper.setRawDataCallback((data: Uint8Array): void => {
      this.rawDataHex = bytesToHex(data);
    });

    // 设置服务列表回调（用于 SSAP 调试显示）
    sleHelper.setServicesCallback((services: ssap.Service[]): void => {
      this.serviceList = services;
      this.appendLog(`发现 ${services.length} 个服务`);
      for (let i: number = 0; i < services.length; i++) {
        const svc: ssap.Service = services[i];
        this.appendLog(`  服务: ${svc.serviceUuid}`);
        if (svc.properties) {
          for (let j: number = 0; j < svc.properties.length; j++) {
            const prop: ssap.Property = svc.properties[j];
            this.appendLog(`    属性: ${prop.propertyUuid} op=${prop.operation}`);
          }
        }
      }
    });

    // 参考示例: connectToDevice(mac, connectedCb, ...)
    sleHelper.connect(this.deviceMac, (ok: boolean) => {
      this.connecting = false;
      this.connected = ok;

      if (ok) {
        this.appendLog(`${this.deviceName} 已连接`);
        // 订阅传感器数据
        this.subscribeSensor();
        if (sleHelper.sendHeartbeat()) {
          this.appendLog('已请求当前设备数据');
        }
      } else {
        this.appendLog('连接失败');
      }
    }, this.deviceName);
  }

  private subscribeSensor(): void {
    sleHelper.subscribeSensorData(this.deviceMac, (data: SensorData) => {
      if (data.hasTempHumi) {
        // 温度趋势
        if (this.lastTemp > 0) {
          if (data.temp100 > this.lastTemp) this.tempTrend = '↑';
          else if (data.temp100 < this.lastTemp) this.tempTrend = '↓';
          else this.tempTrend = '—';
        }
        this.lastTemp = data.temp100;
        this.temperatureText = formatTemp(data.temp100);

        // 湿度趋势
        if (this.lastHumi > 0) {
          if (data.humi100 > this.lastHumi) this.humiTrend = '↑';
          else if (data.humi100 < this.lastHumi) this.humiTrend = '↓';
          else this.humiTrend = '—';
        }
        this.lastHumi = data.humi100;
        this.humidityText = formatHumi(data.humi100);
      }

      this.presenceValue = 1;
      this.presenceText = '人在';

    });
  }

  // ===== 控制命令发送 =====

  /** 添加调试日志（参考示例 appendLog） */
  private appendLog(text: string): void {
    const time: string = new Date().toLocaleTimeString();
    this.debugLog = `[${time}] ${text}\n` + this.debugLog;
  }

  private toggleFan(): void {
    this.setFanPower(!this.fanOn);
  }

  private setFanPower(on: boolean): void {
    try {
      this.fanOn = on;
      this.sendFanControl();
    } catch (err) {
      this.appendLog('风扇控制异常，已拦截');
    }
  }

  private setFanSpeed(speed: FanSpeed): void {
    try {
      this.fanSpeed = speed;
      if (!this.fanOn) {
        this.fanOn = true;
      }
      this.sendFanControl();
    } catch (err) {
      this.appendLog('风扇档位异常，已拦截');
    }
  }

  private sendFanControl(): void {
    if (!this.connected) {
      this.appendLog(`设备未连接，风扇已切换为${this.fanOn ? this.fanSpeedLabel : '关闭'}，未发送`);
      return;
    }

    const ok: boolean = sleHelper.sendControl(this.deviceMac, {
      type: ControlType.FAN,
      mode: this.fanOn ? 1 : 0,
      value: this.fanOn ? this.fanSpeedValue : 0
    });
    this.appendLog(`风扇 ${this.fanOn ? this.fanSpeedLabel : '关闭'} → ${ok ? '已发送' : '发送失败'}`);
  }

  private get fanSpeedValue(): number {
    switch (this.fanSpeed) {
      case FanSpeed.LOW: return 33;
      case FanSpeed.MEDIUM: return 66;
      case FanSpeed.HIGH: return 100;
      default: return 66;
    }
  }

  private get fanSpeedLabel(): string {
    switch (this.fanSpeed) {
      case FanSpeed.LOW: return '低速';
      case FanSpeed.MEDIUM: return '中速';
      case FanSpeed.HIGH: return '高速';
      default: return '中速';
    }
  }

  private toggleLight(): void {
    this.setLightPower(!this.lightOn);
  }

  private setLightPower(on: boolean): void {
    try {
      this.lightOn = on;
      this.sendLightControl();
    } catch (err) {
      this.appendLog('灯光控制异常，已拦截');
    }
  }

  private setLightMode(mode: LightMode): void {
    try {
      this.lightMode = mode;
      if (!this.lightOn) {
        this.lightOn = true;
      }
      this.sendLightControl();
    } catch (err) {
      this.appendLog('灯光档位异常，已拦截');
    }
  }

  private sendLightControl(): void {
    if (!this.connected) {
      this.appendLog(`设备未连接，灯光已切换为${this.lightOn ? this.lightModeLabel : '关闭'}，未发送`);
      return;
    }

    const ok: boolean = sleHelper.sendControl(this.deviceMac, {
      type: ControlType.LIGHT,
      mode: this.lightOn ? 1 : 0,
      value: this.lightOn ? this.lightModeValue : 0
    });
    this.appendLog(`灯光 ${this.lightOn ? this.lightModeLabel : '关闭'} → ${ok ? '已发送' : '发送失败'}`);
  }

  private get lightModeLabel(): string {
    switch (this.lightMode) {
      case LightMode.COLD: return '冷光';
      case LightMode.WARM: return '暖光';
      case LightMode.DIM: return '暗光';
      default: return '关闭';
    }
  }

  private get lightModeValue(): number {
    switch (this.lightMode) {
      case LightMode.DIM: return 33;
      case LightMode.WARM: return 66;
      case LightMode.COLD: return 100;
      default: return 0;
    }
  }

  private get sceneModeLabel(): string {
    switch (this.sceneMode) {
      case SceneMode.HOME: return '居家';
      case SceneMode.AWAY: return '离家';
      case SceneMode.SLEEP: return '睡眠';
      default: return '居家';
    }
  }

  private isFanNeededByEnvironment(): boolean {
    return this.lastTemp >= this.fanTempThreshold || this.lastHumi >= this.fanHumiThreshold;
  }

  private setSceneMode(mode: SceneMode): void {
    try {
      this.sceneMode = mode;
      this.applySceneMode();
      if (this.connected) {
        const ok: boolean = sleHelper.sendSceneMode(this.sceneModeValue);
        this.appendLog(`场景切换为${this.sceneModeLabel} → ${ok ? '已发送' : '发送失败'}`);
      } else {
        this.appendLog(`设备未连接，场景已切换为${this.sceneModeLabel}，未发送`);
      }
    } catch (err) {
      this.appendLog('场景控制异常，已拦截');
    }
  }

  private syncSceneAutomation(): void {
    this.applySceneMode();
  }

  private applySceneMode(): void {
    const fanNeeded: boolean = this.isFanNeededByEnvironment();

    if (this.sceneMode === SceneMode.HOME) {
      this.lightOn = true;
      if (this.lightMode === LightMode.OFF) {
        this.lightMode = LightMode.WARM;
      }
      this.fanOn = fanNeeded;
    } else if (this.sceneMode === SceneMode.AWAY) {
      this.lightOn = false;
      this.fanOn = false;
    } else if (this.sceneMode === SceneMode.SLEEP) {
      this.lightOn = true;
      this.lightMode = LightMode.DIM;
      this.fanOn = fanNeeded;
    }
  }

  private get sceneModeValue(): number {
    switch (this.sceneMode) {
      case SceneMode.HOME: return 1;
      case SceneMode.AWAY: return 2;
      case SceneMode.SLEEP: return 3;
      default: return 1;
    }
  }

  build(): void {
    Column() {
      // ===== 顶部导航栏 =====
      Row() {
        Text('←')
          .fontSize(24)
          .fontColor(AppColors.TEXT_WHITE)
          .onClick(() => {
            try {
              router.back();
            } catch (e) {
              console.error('Router back failed');
            }
          })

        Column() {
          Text(this.deviceName)
            .fontSize(AppFontSize.XL)
            .fontWeight(FontWeight.Bold)
            .fontColor(AppColors.TEXT_WHITE)
          Row() {
            Circle({ width: 6, height: 6 })
              .fill(this.connected ? '#4ADE80' : (this.connecting ? '#FBBF24' : '#F87171'))
            Text(this.connecting ? '连接中...' : (this.connected ? '已连接' : '未连接'))
              .fontSize(AppFontSize.XS)
              .fontColor('rgba(255,255,255,0.85)')
              .margin({ left: 4 })
          }
          .margin({ top: 2 })
        }
        .alignItems(HorizontalAlign.Start)
        .margin({ left: AppSpacing.MD })

        Blank()

        Text('⚙')
          .fontSize(22)
          .fontColor(AppColors.TEXT_WHITE)
      }
      .width('100%')
      .height(56)
      .padding({ left: AppSpacing.LG, right: AppSpacing.LG })
      .linearGradient({
        angle: 135,
        colors: [[AppColors.GRADIENT_HEADER_START, 0.0], [AppColors.GRADIENT_HEADER_END, 1.0]]
      })

      // ===== 连接中状态 =====
      if (this.connecting) {
        Column() {
          LoadingProgress()
            .width(48)
            .height(48)
            .color(AppColors.PRIMARY)
          Text('正在连接设备...')
            .fontSize(AppFontSize.MD)
            .fontColor(AppColors.TEXT_SECONDARY)
            .margin({ top: AppSpacing.MD })
          Text(this.deviceMac)
            .fontSize(AppFontSize.SM)
            .fontColor(AppColors.TEXT_HINT)
            .margin({ top: AppSpacing.XS })
        }
        .width('100%')
        .layoutWeight(1)
        .justifyContent(FlexAlign.Center)
      }

      // ===== 主内容 =====
      if (!this.connecting) {
        Scroll() {
          Column() {
            // ===== 温湿度卡片 =====
            Row() {
              // 温度卡片
              Stack({ alignContent: Alignment.TopStart }) {
                Column() {
                  Row() {
                    Text('🌡')
                      .fontSize(28)
                    Text('温度')
                      .fontSize(AppFontSize.MD)
                      .fontColor(AppColors.TEXT_WHITE)
                      .margin({ left: AppSpacing.XS })
                  }

                  Blank()

                  Row() {
                    Text(this.temperatureText)
                      .fontSize(AppFontSize.MASSIVE)
                      .fontWeight(FontWeight.Bold)
                      .fontColor(AppColors.TEXT_WHITE)
                    Text('℃')
                      .fontSize(AppFontSize.XL)
                      .fontColor('rgba(255,255,255,0.8)')
                      .margin({ left: 4, bottom: 8 })
                    if (this.tempTrend !== '') {
                      Text(this.tempTrend)
                        .fontSize(AppFontSize.LG)
                        .fontColor('rgba(255,255,255,0.9)')
                        .margin({ left: AppSpacing.XS, bottom: 8 })
                    }
                  }
                  .alignItems(VerticalAlign.Bottom)
                }
                .width('100%')
                .height('100%')
                .padding(AppSpacing.LG)
                .alignItems(HorizontalAlign.Start)
                .justifyContent(FlexAlign.SpaceBetween)
              }
              .layoutWeight(1)
              .height(160)
              .borderRadius(AppRadius.LG)
              .linearGradient({
                angle: 135,
                colors: [[AppColors.GRADIENT_TEMP_START, 0.0], [AppColors.GRADIENT_TEMP_END, 1.0]]
              })
              .shadow({ radius: 8, color: 'rgba(255,87,34,0.2)', offsetX: 0, offsetY: 4 })

              // 间距
              Blank().width(AppSpacing.MD)

              // 湿度卡片
              Stack({ alignContent: Alignment.TopStart }) {
                Column() {
                  Row() {
                    Text('💧')
                      .fontSize(28)
                    Text('湿度')
                      .fontSize(AppFontSize.MD)
                      .fontColor(AppColors.TEXT_WHITE)
                      .margin({ left: AppSpacing.XS })
                  }

                  Blank()

                  Row() {
                    Text(this.humidityText)
                      .fontSize(AppFontSize.MASSIVE)
                      .fontWeight(FontWeight.Bold)
                      .fontColor(AppColors.TEXT_WHITE)
                    Text('%')
                      .fontSize(AppFontSize.XL)
                      .fontColor('rgba(255,255,255,0.8)')
                      .margin({ left: 4, bottom: 8 })
                    if (this.humiTrend !== '') {
                      Text(this.humiTrend)
                        .fontSize(AppFontSize.LG)
                        .fontColor('rgba(255,255,255,0.9)')
                        .margin({ left: AppSpacing.XS, bottom: 8 })
                    }
                  }
                  .alignItems(VerticalAlign.Bottom)
                }
                .width('100%')
                .height('100%')
                .padding(AppSpacing.LG)
                .alignItems(HorizontalAlign.Start)
                .justifyContent(FlexAlign.SpaceBetween)
              }
              .layoutWeight(1)
              .height(160)
              .borderRadius(AppRadius.LG)
              .linearGradient({
                angle: 135,
                colors: [[AppColors.GRADIENT_HUMI_START, 0.0], [AppColors.GRADIENT_HUMI_END, 1.0]]
              })
              .shadow({ radius: 8, color: 'rgba(2,136,209,0.2)', offsetX: 0, offsetY: 4 })
            }
            .width('100%')
            .margin({ top: AppSpacing.XL })

            // ===== 人体存在传感器 =====
            Column() {
              Row() {
                Stack() {
                  Text(this.presenceValue === 1 ? '🧍' : '🚪')
                    .fontSize(28)
                }
                .width(52)
                .height(52)
                .borderRadius(AppRadius.MD)
                .backgroundColor(this.presenceValue === 1 ? '#DCFCE7' : AppColors.BG_CARD_DARK)
                .alignContent(Alignment.Center)

                Column() {
                  Text('人体存在')
                    .fontSize(AppFontSize.LG)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(AppColors.TEXT_PRIMARY)
                  Text('人在传感器状态')
                    .fontSize(AppFontSize.XS)
                    .fontColor(AppColors.TEXT_SECONDARY)
                    .margin({ top: 2 })
                }
                .alignItems(HorizontalAlign.Start)
                .margin({ left: AppSpacing.MD })

                Blank()

                Text(this.presenceText)
                  .fontSize(AppFontSize.XL)
                  .fontWeight(FontWeight.Bold)
                  .fontColor(this.presenceValue === 1 ? AppColors.SUCCESS : AppColors.TEXT_SECONDARY)
              }
              .width('100%')
            }
            .width('100%')
            .padding(AppSpacing.LG)
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .margin({ top: AppSpacing.MD })
            .shadow({ radius: 4, color: 'rgba(0,0,0,0.05)', offsetX: 0, offsetY: 2 })

            // 数据刷新提示
            Row() {
              if (this.connected) {
                LoadingProgress().width(12).height(12).color(AppColors.TEXT_HINT)
              } else {
                Circle({ width: 8, height: 8 })
                  .fill(AppColors.TEXT_HINT)
              }
              Text(this.connected ? '数据实时更新 · Notify 推送' : '未连接设备，当前显示为空数据')
                .fontSize(AppFontSize.XS)
                .fontColor(AppColors.TEXT_HINT)
                .margin({ left: AppSpacing.XS })
            }
            .width('100%')
            .justifyContent(FlexAlign.Center)
            .margin({ top: AppSpacing.MD })

            // ===== 场景模式 =====
            Column() {
              Row() {
                Text('场景模式')
                  .fontSize(AppFontSize.LG)
                  .fontWeight(FontWeight.Bold)
                  .fontColor(AppColors.TEXT_PRIMARY)
                Blank()
                Text(this.sceneModeLabel)
                  .fontSize(AppFontSize.SM)
                  .fontColor(AppColors.PRIMARY)
              }
              .width('100%')

              Row() {
                this.buildSceneButton('居家', SceneMode.HOME)
                Blank().width(AppSpacing.SM)
                this.buildSceneButton('离家', SceneMode.AWAY)
                Blank().width(AppSpacing.SM)
                this.buildSceneButton('睡眠', SceneMode.SLEEP)
              }
              .width('100%')
              .margin({ top: AppSpacing.MD })

              Text('居家/睡眠在温度≥30℃时自动开风扇；离家关闭风扇和灯光')
                .fontSize(AppFontSize.XS)
                .fontColor(AppColors.TEXT_HINT)
                .margin({ top: AppSpacing.SM })
            }
            .width('100%')
            .padding(AppSpacing.LG)
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .margin({ top: AppSpacing.XL, bottom: AppSpacing.MD })
            .shadow({ radius: 4, color: 'rgba(0,0,0,0.05)', offsetX: 0, offsetY: 2 })

            // ===== 设备控制标题 =====
            Row() {
              Text('设备控制')
                .fontSize(AppFontSize.LG)
                .fontWeight(FontWeight.Bold)
                .fontColor(AppColors.TEXT_PRIMARY)
            }
            .width('100%')
            .margin({ top: AppSpacing.XXL, bottom: AppSpacing.MD })

            // ===== 风扇控制卡片 =====
            Column() {
              Row() {
                Stack() {
                  Text('🌀')
                    .fontSize(26)
                }
                .width(48)
                .height(48)
                .borderRadius(AppRadius.MD)
                .linearGradient({
                  angle: 135,
                  colors: [[AppColors.GRADIENT_FAN_START, 0.0], [AppColors.GRADIENT_FAN_END, 1.0]]
                })
                .alignContent(Alignment.Center)

                Column() {
                  Text('风扇')
                    .fontSize(AppFontSize.LG)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(AppColors.TEXT_PRIMARY)
                  Text(this.fanOn ? this.fanSpeedLabel : '已关闭')
                    .fontSize(AppFontSize.XS)
                    .fontColor(this.fanOn ? AppColors.TEXT_SECONDARY : AppColors.TEXT_HINT)
                    .margin({ top: 2 })
                }
                .alignItems(HorizontalAlign.Start)
                .margin({ left: AppSpacing.MD })

                Blank()

                Toggle({ type: ToggleType.Switch, isOn: this.fanOn })
                  .selectedColor(AppColors.SUCCESS)
                  .onChange((on: boolean) => this.setFanPower(on))
              }
              .width('100%')

              Row() {
                this.buildFanSpeedButton('低档', FanSpeed.LOW)
                Blank().width(AppSpacing.SM)
                this.buildFanSpeedButton('中档', FanSpeed.MEDIUM)
                Blank().width(AppSpacing.SM)
                this.buildFanSpeedButton('高档', FanSpeed.HIGH)
              }
              .width('100%')
              .margin({ top: AppSpacing.MD })
            }
            .width('100%')
            .padding(AppSpacing.LG)
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .margin({ bottom: AppSpacing.MD })
            .shadow({ radius: 4, color: 'rgba(0,0,0,0.05)', offsetX: 0, offsetY: 2 })

            // ===== 灯光控制卡片 =====
            Column() {
              Row() {
                // 图标
                Stack() {
                  Text('💡')
                    .fontSize(26)
                }
                .width(48)
                .height(48)
                .borderRadius(AppRadius.MD)
                .linearGradient({
                  angle: 135,
                  colors: [[AppColors.GRADIENT_LIGHT_START, 0.0], [AppColors.GRADIENT_LIGHT_END, 1.0]]
                })
                .alignContent(Alignment.Center)

                // 名称和状态
                Column() {
                  Text('灯光')
                    .fontSize(AppFontSize.LG)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(AppColors.TEXT_PRIMARY)
                  Text(this.lightOn ? this.lightModeLabel : '已关闭')
                    .fontSize(AppFontSize.XS)
                    .fontColor(AppColors.TEXT_SECONDARY)
                    .margin({ top: 2 })
                }
                .alignItems(HorizontalAlign.Start)
                .margin({ left: AppSpacing.MD })

                Blank()

                // 开关
                Toggle({ type: ToggleType.Switch, isOn: this.lightOn })
                  .selectedColor(AppColors.SUCCESS)
                  .onChange((on: boolean) => this.setLightPower(on))
              }
              .width('100%')

              Row() {
                this.buildLightModeButton('冷光', LightMode.COLD)
                Blank().width(AppSpacing.SM)
                this.buildLightModeButton('暖光', LightMode.WARM)
                Blank().width(AppSpacing.SM)
                this.buildLightModeButton('暗光', LightMode.DIM)
              }
              .width('100%')
              .margin({ top: AppSpacing.MD })
            }
            .width('100%')
            .padding(AppSpacing.LG)
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .margin({ bottom: AppSpacing.MD })
            .shadow({ radius: 4, color: 'rgba(0,0,0,0.05)', offsetX: 0, offsetY: 2 })

            // ===== 设备信息 =====
            Column() {
              Text('设备信息')
                .fontSize(AppFontSize.LG)
                .fontWeight(FontWeight.Bold)
                .fontColor(AppColors.TEXT_PRIMARY)
                .margin({ bottom: AppSpacing.MD })

              this.buildInfoRow('设备名称', this.deviceName)
              this.buildInfoRow('设备类型', this.deviceDesc)
              this.buildInfoRow('MAC 地址', this.deviceMac.length > 0 ? this.deviceMac : '暂无')
              this.buildInfoRow('连接方式', '星闪 (NearLink)')
              this.buildInfoRow('通信协议', 'SSAP + UART TLV')
              this.buildInfoRow('数据状态', this.connected ? '实时数据' : '暂无数据')
            }
            .width('100%')
            .padding(AppSpacing.LG)
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .margin({ top: AppSpacing.XL, bottom: AppSpacing.XXL })
            .shadow({ radius: 4, color: 'rgba(0,0,0,0.05)', offsetX: 0, offsetY: 2 })

            // ===== SSAP 调试区域（对应 PPT: SSAP 调试） =====
            Column() {
              // 调试标题栏（可折叠）
              Row() {
                Text('SSAP 调试')
                  .fontSize(AppFontSize.LG)
                  .fontWeight(FontWeight.Bold)
                  .fontColor(AppColors.TEXT_PRIMARY)
                Blank()
                Text(this.showDebug ? '收起 ▲' : '展开 ▼')
                  .fontSize(AppFontSize.SM)
                  .fontColor(AppColors.PRIMARY)
              }
              .width('100%')
              .onClick(() => {
                this.showDebug = !this.showDebug;
              })

              if (this.showDebug) {
                // 原始数据
                Column() {
                  Text('原始数据 (Hex)')
                    .fontSize(AppFontSize.SM)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(AppColors.TEXT_SECONDARY)
                  Text(this.rawDataHex)
                    .fontSize(AppFontSize.XS)
                    .fontColor(AppColors.TEXT_PRIMARY)
                    .fontFamily('monospace')
                    .width('100%')
                    .padding(AppSpacing.SM)
                    .margin({ top: AppSpacing.XS })
                    .backgroundColor(AppColors.BG_CARD_DARK)
                    .borderRadius(AppRadius.SM)
                }
                .width('100%')
                .margin({ top: AppSpacing.MD })

                // 服务和属性列表
                Column() {
                  Text('服务 / 属性列表')
                    .fontSize(AppFontSize.SM)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(AppColors.TEXT_SECONDARY)
                  if (this.serviceList.length === 0) {
                    Text('暂未发现服务')
                      .fontSize(AppFontSize.XS)
                      .fontColor(AppColors.TEXT_HINT)
                      .margin({ top: AppSpacing.XS })
                  } else {
                    ForEach(this.serviceList, (svc: ssap.Service) => {
                      Column() {
                        Text(`Service: ${svc.serviceUuid}`)
                          .fontSize(AppFontSize.XS)
                          .fontColor(AppColors.TEXT_PRIMARY)
                          .fontFamily('monospace')
                        if (svc.properties) {
                          ForEach(svc.properties, (prop: ssap.Property) => {
                            Text(`  └ Property: ${prop.propertyUuid}  op=${prop.operation}`)
                              .fontSize(AppFontSize.XS)
                              .fontColor(AppColors.TEXT_SECONDARY)
                              .fontFamily('monospace')
                          })
                        }
                      }
                      .width('100%')
                      .margin({ top: AppSpacing.XS })
                    })
                  }
                }
                .width('100%')
                .margin({ top: AppSpacing.MD })

                // 手动触发心跳
                Row() {
                  Button('发送心跳 (触发上报)')
                    .fontSize(AppFontSize.SM)
                    .type(ButtonType.Normal)
                    .borderRadius(AppRadius.SM)
                    .backgroundColor(AppColors.PRIMARY)
                    .fontColor(AppColors.TEXT_WHITE)
                    .layoutWeight(1)
                    .onClick(() => {
                      if (sleHelper.sendHeartbeat()) {
                        this.appendLog('已发送 HEARTBEAT');
                      } else {
                        this.appendLog('发送 HEARTBEAT 失败');
                      }
                    })
                }
                .width('100%')
                .margin({ top: AppSpacing.MD })

                // 调试日志
                Column() {
                  Text('调试日志')
                    .fontSize(AppFontSize.SM)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(AppColors.TEXT_SECONDARY)
                  Scroll() {
                    Text(this.debugLog)
                      .fontSize(AppFontSize.XS)
                      .fontColor(AppColors.TEXT_PRIMARY)
                      .fontFamily('monospace')
                      .width('100%')
                  }
                  .width('100%')
                  .height(120)
                  .padding(AppSpacing.SM)
                  .margin({ top: AppSpacing.XS })
                  .backgroundColor(AppColors.BG_CARD_DARK)
                  .borderRadius(AppRadius.SM)
                  .scrollBar(BarState.Auto)
                }
                .width('100%')
                .margin({ top: AppSpacing.MD })
              }
            }
            .width('100%')
            .padding(AppSpacing.LG)
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .margin({ top: AppSpacing.XL, bottom: AppSpacing.XXL })
            .shadow({ radius: 4, color: 'rgba(0,0,0,0.05)', offsetX: 0, offsetY: 2 })

            // 底部留白
            Blank().height(40)
          }
          .width('100%')
          .padding({ left: AppSpacing.XXL, right: AppSpacing.XXL })
        }
        .layoutWeight(1)
        .scrollBar(BarState.Off)
        .align(Alignment.Top)
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor(AppColors.BG_PAGE)
  }

  @Builder
  buildSceneButton(label: string, mode: SceneMode): void {
    Button(label)
      .fontSize(AppFontSize.SM)
      .fontColor(this.sceneMode === mode ? AppColors.TEXT_WHITE : AppColors.TEXT_PRIMARY)
      .backgroundColor(this.sceneMode === mode ? AppColors.PRIMARY : AppColors.BG_CARD_DARK)
      .borderRadius(AppRadius.SM)
      .layoutWeight(1)
      .height(38)
      .onClick(() => this.setSceneMode(mode))
  }

  @Builder
  buildFanSpeedButton(label: string, speed: FanSpeed): void {
    Button(label)
      .fontSize(AppFontSize.SM)
      .fontColor(this.fanOn && this.fanSpeed === speed ? AppColors.TEXT_WHITE : AppColors.TEXT_PRIMARY)
      .backgroundColor(this.fanOn && this.fanSpeed === speed ? AppColors.PRIMARY : AppColors.BG_CARD_DARK)
      .borderRadius(AppRadius.SM)
      .layoutWeight(1)
      .height(38)
      .onClick(() => this.setFanSpeed(speed))
  }

  @Builder
  buildLightModeButton(label: string, mode: LightMode): void {
    Button(label)
      .fontSize(AppFontSize.SM)
      .fontColor(this.lightOn && this.lightMode === mode ? AppColors.TEXT_WHITE : AppColors.TEXT_PRIMARY)
      .backgroundColor(this.lightOn && this.lightMode === mode ? AppColors.WARNING : AppColors.BG_CARD_DARK)
      .borderRadius(AppRadius.SM)
      .layoutWeight(1)
      .height(38)
      .onClick(() => this.setLightMode(mode))
  }

  @Builder
  buildInfoRow(label: string, value: string): void {
    Row() {
      Text(label)
        .fontSize(AppFontSize.SM)
        .fontColor(AppColors.TEXT_SECONDARY)
      Blank()
      Text(value)
        .fontSize(AppFontSize.SM)
        .fontColor(AppColors.TEXT_PRIMARY)
        .fontWeight(FontWeight.Medium)
    }
    .width('100%')
    .padding({ top: AppSpacing.SM, bottom: AppSpacing.SM })
    .border({ width: { bottom: 1 }, color: AppColors.DIVIDER })
  }
}

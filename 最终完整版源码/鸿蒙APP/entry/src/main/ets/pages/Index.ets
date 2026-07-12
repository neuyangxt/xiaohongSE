import router from '@ohos.router';
import { DeviceInfo, DeviceType } from '../model/DeviceModel';
import { AppColors, AppRadius, AppSpacing, AppFontSize } from '../common/Theme';
import { sleHelper } from '../sle/helper';
import { promptAction } from '@kit.ArkUI';

/**
 * 扫描列表页面
 * 对应 PPT: pages/Index.ets — 扫描列表
 *
 * 流程：
 *   1. 用户点击「开始扫描」
 *   2. App 检查星闪权限并开始扫描
 *   3. 列表中出现 XH_HOME 等设备
 *   4. 点击设备进入 Device 页
 */
@Entry
@Component
struct Index {
  @State scanning: boolean = false;
  @State devices: DeviceInfo[] = [];
  @State nearLinkEnabled: boolean = true;
  @State permissionGranted: boolean = false;
  private scanStateTimer: number = -1;

  aboutToAppear(): void {
    this.initNearLink();
  }

  aboutToDisappear(): void {
    sleHelper.stopScan();
    this.clearScanStateTimer();
  }

  /** 页面隐藏时停止扫描（参考示例 onPageHide） */
  onPageHide(): void {
    sleHelper.stopScan();
    this.clearScanStateTimer();
    this.scanning = false;
  }

  /** 页面显示时清空列表（参考示例 onPageShow） */
  onPageShow(): void {
    this.devices = [];
  }

  private async initNearLink(): Promise<void> {
    this.permissionGranted = await sleHelper.checkAndRequestPermission();
    if (this.permissionGranted) {
      this.nearLinkEnabled = sleHelper.isNearLinkEnabled();
    }
  }

  private clearScanStateTimer(): void {
    if (this.scanStateTimer !== -1) {
      clearInterval(this.scanStateTimer);
      this.scanStateTimer = -1;
    }
  }

  /** 弹出提示 */
  private showToast(msg: string): void {
    try {
      promptAction.showToast({
        message: msg,
        duration: 3000,
        bottom: 200,
      });
    } catch (e) {
      console.error('showToast failed');
    }
  }

  private async onStartScan(): Promise<void> {
    // 参考示例: 点击扫描中按钮则停止扫描
    if (this.scanning) {
      sleHelper.stopScan();
      this.scanning = false;
      this.clearScanStateTimer();
      return;
    }

    // 检查权限（参考示例: onGetGrantStatus）
    if (!this.permissionGranted) {
      this.permissionGranted = await sleHelper.checkAndRequestPermission();
      if (!this.permissionGranted) {
        this.showToast('星闪权限未授予，无法扫描');
        return;
      }
    }

    // 检查星闪开关（参考示例: onSleState）
    if (!this.nearLinkEnabled) {
      this.nearLinkEnabled = sleHelper.isNearLinkEnabled();
      if (!this.nearLinkEnabled) {
        this.showToast('星闪未开启。模拟器不支持星闪，请使用真机调试');
        return;
      }
    }

    this.devices = [];
    this.scanning = true;

    // 参考示例: startSleScan，helper 内部有 20s 超时
    sleHelper.startScan((found: DeviceInfo[]) => {
      this.devices = found;
    });

    // 定期同步扫描状态（helper 20s 后自动停止）
    this.scanStateTimer = setInterval((): void => {
      if (!sleHelper.isScanning()) {
        this.scanning = false;
        this.clearScanStateTimer();
      }
    }, 500);
  }

  private onDeviceClick(device: DeviceInfo): void {
    sleHelper.stopScan();
    this.scanning = false;

    try {
      router.pushUrl({
        url: 'pages/Device',
        params: {
          name: device.name,
          mac: device.mac,
          type: device.type,
          description: device.description
        }
      });
    } catch (e) {
      console.error('Router pushUrl failed');
    }
  }

  private onPreviewPanelClick(): void {
    sleHelper.stopScan();
    this.scanning = false;

    try {
      router.pushUrl({
        url: 'pages/Device',
        params: {
          name: '未连接设备',
          mac: '',
          type: DeviceType.HUB,
          description: '空数据预览'
        }
      });
    } catch (e) {
      console.error('Router pushUrl failed');
    }
  }

  /** 获取设备类型图标 */
  private getDeviceIcon(type: DeviceType): string {
    switch (type) {
      case DeviceType.HUB: return '🏠';
      case DeviceType.SENSOR: return '🌡';
      case DeviceType.LIGHT: return '💡';
      case DeviceType.FAN: return '🌀';
      default: return '📦';
    }
  }

  /** 获取信号强度描述 */
  private getRssiLevel(rssi: number): string {
    if (rssi >= -50) return '强';
    if (rssi >= -65) return '中';
    return '弱';
  }

  build(): void {
    Column() {
      // ===== 顶部渐变头部 =====
      Stack({ alignContent: Alignment.TopStart }) {
        Column() {
          // 状态栏占位
          Row().height(44)

          // 标题行
          Row() {
            Column() {
              Text('智能家居中控')
                .fontSize(AppFontSize.XXXL)
                .fontWeight(FontWeight.Bold)
                .fontColor(AppColors.TEXT_WHITE)
              Text('星闪 AIoT 控制中心')
                .fontSize(AppFontSize.SM)
                .fontColor('rgba(255,255,255,0.8)')
                .margin({ top: AppSpacing.XS })
            }
            .alignItems(HorizontalAlign.Start)

            Blank()

            // 星闪状态徽章
            Row() {
              Circle({ width: 8, height: 8 })
                .fill(this.nearLinkEnabled ? '#4ADE80' : '#F87171')
              Text(this.nearLinkEnabled ? '星闪就绪' : '星闪未开启')
                .fontSize(AppFontSize.XS)
                .fontColor(AppColors.TEXT_WHITE)
                .margin({ left: AppSpacing.XS })
            }
            .padding({ left: AppSpacing.SM, right: AppSpacing.SM, top: 6, bottom: 6 })
            .backgroundColor('rgba(255,255,255,0.2)')
            .borderRadius(AppRadius.FULL)
          }
          .width('100%')
          .padding({ left: AppSpacing.XXL, right: AppSpacing.XXL, bottom: AppSpacing.XL })
        }
        .width('100%')
      }
      .width('100%')
      .height(160)
      .linearGradient({
        angle: 135,
        colors: [[AppColors.GRADIENT_HEADER_START, 0.0], [AppColors.GRADIENT_HEADER_END, 1.0]]
      })

      // ===== 内容区域 =====
      Scroll() {
        Column() {
          // 扫描按钮
          Button() {
            Row() {
              if (this.scanning) {
                LoadingProgress()
                  .width(24)
                  .height(24)
                  .color(AppColors.TEXT_WHITE)
                  .margin({ right: AppSpacing.SM })
                Text('扫描中...')
                  .fontSize(AppFontSize.LG)
                  .fontWeight(FontWeight.Medium)
                  .fontColor(AppColors.TEXT_WHITE)
              } else {
                Text('📡')
                  .fontSize(22)
                  .margin({ right: AppSpacing.SM })
                Text('开始扫描')
                  .fontSize(AppFontSize.LG)
                  .fontWeight(FontWeight.Medium)
                  .fontColor(AppColors.TEXT_WHITE)
              }
            }
          }
          .width('100%')
          .height(52)
          .linearGradient({
            angle: 90,
            colors: [[AppColors.PRIMARY, 0.0], [AppColors.ACCENT, 1.0]]
          })
          .borderRadius(AppRadius.LG)
          .shadow({ radius: 12, color: 'rgba(59,130,246,0.3)', offsetX: 0, offsetY: 4 })
          .margin({ top: AppSpacing.XXL })
          .onClick(() => this.onStartScan())

          // 星闪未开启警告（模拟器/未开星闪时显示）
          if (!this.nearLinkEnabled) {
            Column() {
              Text('⚠️')
                .fontSize(48)
                .margin({ bottom: AppSpacing.MD })
              Text('星闪不可用')
                .fontSize(AppFontSize.LG)
                .fontWeight(FontWeight.Bold)
                .fontColor(AppColors.DANGER)
                .margin({ bottom: AppSpacing.XS })
              Text('模拟器不支持星闪功能，请使用支持星闪的真机调试。')
                .fontSize(AppFontSize.SM)
                .fontColor(AppColors.TEXT_SECONDARY)
                .textAlign(TextAlign.Center)
              Text('真机需在「设置 → 星闪」中打开星闪开关。')
                .fontSize(AppFontSize.SM)
                .fontColor(AppColors.TEXT_HINT)
                .margin({ top: AppSpacing.XS, bottom: AppSpacing.LG })
                .textAlign(TextAlign.Center)

              this.buildPreviewPanelEntry()
            }
            .width('100%')
            .padding(AppSpacing.XXL)
            .margin({ top: 40 })
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .border({ width: 1, color: AppColors.DANGER })
          }

          // 提示信息（星闪可用但未扫描时显示）
          if (this.nearLinkEnabled && !this.scanning && this.devices.length === 0) {
            Column() {
              Text('🔍')
                .fontSize(48)
                .margin({ bottom: AppSpacing.MD })
              Text('暂无设备')
                .fontSize(AppFontSize.LG)
                .fontColor(AppColors.TEXT_SECONDARY)
                .margin({ bottom: AppSpacing.XS })
              Text('点击上方按钮扫描附近的星闪设备')
                .fontSize(AppFontSize.SM)
                .fontColor(AppColors.TEXT_HINT)
                .margin({ bottom: AppSpacing.LG })

              this.buildPreviewPanelEntry()
            }
            .width('100%')
            .margin({ top: 60 })
          }

          // 扫描中空状态
          if (this.scanning && this.devices.length === 0) {
            Column() {
              Text('📡')
                .fontSize(48)
                .margin({ bottom: AppSpacing.MD })
              Text('正在搜索设备...')
                .fontSize(AppFontSize.LG)
                .fontColor(AppColors.TEXT_SECONDARY)
              Text('请确保设备已通电并广播信号')
                .fontSize(AppFontSize.SM)
                .fontColor(AppColors.TEXT_HINT)
                .margin({ top: AppSpacing.XS })
              Text('如果暂时扫不到，也可以先查看空数据控制界面')
                .fontSize(AppFontSize.SM)
                .fontColor(AppColors.TEXT_HINT)
                .margin({ top: AppSpacing.XS, bottom: AppSpacing.LG })

              this.buildPreviewPanelEntry()
            }
            .width('100%')
            .margin({ top: 60 })
          }

          // 设备列表标题
          if (this.devices.length > 0) {
            Row() {
              Text('已发现设备')
                .fontSize(AppFontSize.MD)
                .fontWeight(FontWeight.Medium)
                .fontColor(AppColors.TEXT_PRIMARY)
              Text(`(${this.devices.length})`)
                .fontSize(AppFontSize.MD)
                .fontColor(AppColors.TEXT_SECONDARY)
                .margin({ left: AppSpacing.XS })
              Blank()
              if (this.scanning) {
                Row() {
                  LoadingProgress().width(14).height(14).color(AppColors.PRIMARY)
                  Text('扫描中')
                    .fontSize(AppFontSize.XS)
                    .fontColor(AppColors.PRIMARY)
                    .margin({ left: 4 })
                }
              }
            }
            .width('100%')
            .margin({ top: AppSpacing.XXL, bottom: AppSpacing.MD })

            // 设备列表
            ForEach(this.devices, (device: DeviceInfo) => {
              Row() {
                // 设备图标
                Stack() {
                  Text(this.getDeviceIcon(device.type))
                    .fontSize(28)
                }
                .width(52)
                .height(52)
                .borderRadius(AppRadius.MD)
                .backgroundColor(AppColors.BG_CARD_DARK)
                .alignContent(Alignment.Center)

                // 设备信息
                Column() {
                  Row() {
                    Text(device.name)
                      .fontSize(AppFontSize.LG)
                      .fontWeight(FontWeight.Bold)
                      .fontColor(AppColors.TEXT_PRIMARY)
                    Text(device.description)
                      .fontSize(AppFontSize.XS)
                      .fontColor(AppColors.TEXT_WHITE)
                      .backgroundColor(AppColors.PRIMARY)
                      .borderRadius(AppRadius.SM)
                      .padding({ left: 6, right: 6, top: 2, bottom: 2 })
                      .margin({ left: AppSpacing.SM })
                  }
                  .alignItems(VerticalAlign.Center)

                  Text(`MAC: ${device.mac}`)
                    .fontSize(AppFontSize.XS)
                    .fontColor(AppColors.TEXT_HINT)
                    .margin({ top: AppSpacing.XS })

                  Row() {
                    Text(`信号 ${device.rssi}dBm`)
                      .fontSize(AppFontSize.XS)
                      .fontColor(AppColors.TEXT_SECONDARY)
                    Text('·')
                      .fontSize(AppFontSize.XS)
                      .fontColor(AppColors.TEXT_HINT)
                      .margin({ left: AppSpacing.XS, right: AppSpacing.XS })
                    Text(`强度: ${this.getRssiLevel(device.rssi)}`)
                      .fontSize(AppFontSize.XS)
                      .fontColor(AppColors.TEXT_SECONDARY)
                  }
                  .margin({ top: 2 })
                }
                .alignItems(HorizontalAlign.Start)
                .layoutWeight(1)
                .margin({ left: AppSpacing.MD })

                // 箭头
                Text('›')
                  .fontSize(24)
                  .fontColor(AppColors.TEXT_HINT)
              }
              .width('100%')
              .padding(AppSpacing.LG)
              .backgroundColor(AppColors.BG_CARD)
              .borderRadius(AppRadius.LG)
              .margin({ bottom: AppSpacing.MD })
              .onClick(() => this.onDeviceClick(device))
            }, (device: DeviceInfo) => device.mac)
          }

          // 底部说明
          if (this.devices.length > 0) {
            Column() {
              Text('使用说明')
                .fontSize(AppFontSize.MD)
                .fontWeight(FontWeight.Medium)
                .fontColor(AppColors.TEXT_PRIMARY)
                .margin({ bottom: AppSpacing.SM })

              this.buildTipItem('1', '确保手机星闪开关已打开')
              this.buildTipItem('2', '确保设备已通电并处于广播状态')
              this.buildTipItem('3', '点击设备进入控制页面')
            }
            .width('100%')
            .padding(AppSpacing.XL)
            .backgroundColor(AppColors.BG_CARD)
            .borderRadius(AppRadius.LG)
            .margin({ top: AppSpacing.XL, bottom: AppSpacing.XXL })
          }
        }
        .width('100%')
        .padding({ left: AppSpacing.XXL, right: AppSpacing.XXL })
      }
      .layoutWeight(1)
      .scrollBar(BarState.Off)
      .align(Alignment.Top)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(AppColors.BG_PAGE)
  }

  @Builder
  buildTipItem(num: string, text: string): void {
    Row() {
      Text(num)
        .fontSize(AppFontSize.SM)
        .fontWeight(FontWeight.Bold)
        .fontColor(AppColors.TEXT_WHITE)
        .textAlign(TextAlign.Center)
        .width(20)
        .height(20)
        .borderRadius(10)
        .backgroundColor(AppColors.PRIMARY)
      Text(text)
        .fontSize(AppFontSize.SM)
        .fontColor(AppColors.TEXT_SECONDARY)
        .margin({ left: AppSpacing.SM })
    }
    .margin({ bottom: AppSpacing.SM })
  }

  @Builder
  buildPreviewPanelEntry(): void {
    Row() {
      Stack() {
        Text('🏠')
          .fontSize(28)
      }
      .width(52)
      .height(52)
      .borderRadius(AppRadius.MD)
      .backgroundColor(AppColors.BG_CARD_DARK)
      .alignContent(Alignment.Center)

      Column() {
        Text('查看控制面板')
          .fontSize(AppFontSize.LG)
          .fontWeight(FontWeight.Bold)
          .fontColor(AppColors.TEXT_PRIMARY)
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)
      .margin({ left: AppSpacing.MD })

      Text('›')
        .fontSize(24)
        .fontColor(AppColors.TEXT_HINT)
    }
    .width('100%')
    .padding(AppSpacing.LG)
    .backgroundColor(AppColors.BG_CARD)
    .borderRadius(AppRadius.LG)
    .shadow({ radius: 4, color: 'rgba(0,0,0,0.05)', offsetX: 0, offsetY: 2 })
    .onClick(() => this.onPreviewPanelClick())
  }
}

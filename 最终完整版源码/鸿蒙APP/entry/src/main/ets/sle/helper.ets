/**
 * 星闪 SLE 助手 — NearLinkKit SSAP 客户端封装
 *
 * 参考官方 phone 示例 (04/06/07) 的 NearLinkKit API 调用模式:
 *   - scan: ScanFilters{deviceName:"XH_HOME"} + ScanOptions{scanMode:2}
 *   - connect: ssap.createClient → on('connectionStateChange') →
 *              STATE_CONNECTED 时 on('propertyChange') + getServices()
 *   - discover: 遍历 services, 按 operation 标志位找到可通知/可读/可写属性
 *   - subscribe: client.setPropertyNotification(property, true)
 *   - poll: setInterval readProperty (500ms)
 *   - write: 找到完整 property 对象 → 设 value → writeProperty(WRITE)
 *   - disconnect: client.close() 取消所有事件订阅
 *
 * 设备端协议参考:
 *   02_sle_multi_module_home/proto/xh_sle_proto.c (XH TLV 协议)
 *   02_sle_multi_module_home/sle/xh_sle_server.c (UUID 0x2222/0x2323)
 */

import { scan, ssap, manager, constant } from '@kit.NearLinkKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { bundleManager, abilityAccessCtrl, common } from '@kit.AbilityKit';

import {
  DeviceInfo, SensorData, ControlCommand, ControlType,
  makeDeviceInfo, parseModuleIdFromName,
} from '../model/DeviceModel';
import {
  XhModuleId, XhTlvType, XhTlv,
  XH_SERVICE_UUID, XH_PROPERTY_UUID,
  encodeControl, encodeHeartbeat, encodeSceneControl,
  parseThPayload, ThData,
  tlvU8, uint8ToArrayBuffer, arrayBufferToUint8, bytesToHex,
} from './XhProto';

const TAG: string = 'SleHelper';

/** 读取轮询间隔 (ms)，与示例一致 500ms */
const READ_INTERVAL: number = 500;

/** 心跳触发上报间隔 (ms)，用于兼容 Notify 不主动推送的固件 */
const HEARTBEAT_INTERVAL: number = 5 * 1000;

/** 扫描超时 (ms)，与示例一致 20s */
const SCAN_TIMEOUT: number = 20 * 1000;

/** 精确名称过滤无结果时，切换到无过滤扫描的等待时间 */
const SCAN_FILTER_FALLBACK_MS: number = 4 * 1000;

/** 扫描模式 2 = 低功耗扫描 */
const SCAN_MODE: number = 2;

/** APP 应连接 Hub；Hub 广播名由 BUILD.gn 定义为 XH_HOME */
const HUB_DEVICE_NAME: string = 'XH_HOME';

/** 参考头文件默认名，避免固件未带 BUILD.gn 宏时被过滤 */
const LEGACY_HUB_DEVICE_NAME: string = 'XH_TH_FAN';

// ===== 连接状态 =====

enum ConnState {
  IDLE = 0,
  CONNECTING = 1,
  CONNECTED = 2,
}

// ===== 单例助手类 =====

class SleHelper {
  // ----- 上下文 -----
  private context: common.UIAbilityContext | null = null;

  // ----- 扫描状态 -----
  private scanning: boolean = false;
  private scanCallback: ((devices: DeviceInfo[]) => void) | null = null;
  private foundDevices: DeviceInfo[] = [];
  private scanTimer: number = -1;
  private scanFallbackTimer: number = -1;

  // ----- 连接状态 -----
  private connState: ConnState = ConnState.IDLE;
  private client: ssap.Client | null = null;
  private connectedMac: string = '';
  private connectedName: string = '';
  private connectedModuleId: number = 0;

  // 发现到的服务和属性（完整对象，用于 read/write/notify）
  private serviceList: ssap.Service[] = [];
  private targetProperty: ssap.Property | null = null;
  private targetServiceUuid: string = '';
  private targetPropertyUuid: string = '';

  // ----- 数据回调 -----
  private sensorCallback: ((data: SensorData) => void) | null = null;
  private rawCallback: ((data: Uint8Array) => void) | null = null;
  private connectCallback: ((connected: boolean) => void) | null = null;
  private servicesCallback: ((services: ssap.Service[]) => void) | null = null;
  private latestSensorData: SensorData | null = null;

  // SSAP 回调（保持引用用于 off）
  private propertyChangeCb: (data: ssap.Property) => void = (data: ssap.Property): void => {
    if (this.connState !== ConnState.CONNECTED) {
      return;
    }
    this.handleReceivedData(data.value);
  };

  // ----- 读取轮询 -----
  private readTimer: number = -1;
  private heartbeatTimer: number = -1;

  // ================================================================
  //  上下文管理
  // ================================================================

  /** 设置 UIAbility 上下文（由 EntryAbility 调用） */
  setContext(context: common.UIAbilityContext): void {
    this.context = context;
  }

  // ================================================================
  //  权限检查（对应 PPT 步骤 1-2，参考示例 getGrantStatus / askUserAuthorize）
  // ================================================================

  /**
   * 检查 ACCESS_NEARLINK 权限是否已授予
   * @returns true 如果权限已授予
   */
  async checkAndRequestPermission(): Promise<boolean> {
    if (!this.context) {
      console.error(`${TAG} context not set`);
      return false;
    }

    try {
      // 1. 获取本应用 tokenId（示例用 await）
      const bundleInfo: bundleManager.BundleInfo =
        await bundleManager.getBundleInfoForSelf(
          bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION,
        );
      const tokenId: number = bundleInfo.appInfo.accessTokenId;

      // 2. 检查权限是否已授予（示例用 await）
      const atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
      const grantStatus: abilityAccessCtrl.GrantStatus =
        await atManager.checkAccessToken(tokenId, 'ohos.permission.ACCESS_NEARLINK');

      if (grantStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        console.info(`${TAG} permission already granted`);
        return true;
      }

      // 3. 申请权限
      console.info(`${TAG} requesting permission...`);
      const result =
        await atManager.requestPermissionsFromUser(
          this.context,
          ['ohos.permission.ACCESS_NEARLINK'],
        );

      if (result.authResults && result.authResults.length > 0) {
        const granted: boolean =
          result.authResults[0] === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED;
        console.info(`${TAG} permission granted: ${granted}`);
        return granted;
      }
      return false;
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} permission check failed: code=${e.code} msg=${e.message}`);
      return false;
    }
  }

  // ================================================================
  //  星闪开关状态（对应 PPT 步骤 3，参考示例 getSleState）
  // ================================================================

  /**
   * 检查星闪是否已开启
   * @returns true 如果星闪已开启
   */
  isNearLinkEnabled(): boolean {
    try {
      const state: manager.NearlinkState = manager.getState();
      console.info(`${TAG} nearlink state: ${state}`);
      return state === manager.NearlinkState.STATE_ON;
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} getState failed: ${e.message}`);
      return false;
    }
  }

  // ================================================================
  //  扫描（对应 PPT 步骤 4，参考示例 startSleScan）
  // ================================================================

  /**
   * 开始扫描星闪设备
   * 参考示例: scan.ScanFilters{deviceName:"XH_HOME"} + ScanOptions{scanMode:2}
   * @param callback 扫描结果回调
   */
  startScan(callback: (devices: DeviceInfo[]) => void): void {
    if (this.scanning) {
      console.info(`${TAG} already scanning`);
      return;
    }

    this.scanCallback = callback;
    this.foundDevices = [];
    this.scanning = true;

    // 先取消旧的 deviceFound 订阅，避免重复回调（参考示例）
    try {
      scan.off('deviceFound');
    } catch (err) {
      // 首次调用 off 可能无注册，忽略
    }

    // 订阅扫描结果（可能抛出异常，需要 try-catch）
    try {
      scan.on('deviceFound', (results: scan.ScanResults[]): void => {
        this.onScanResultInternal(results);
      });
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} scan.on(deviceFound) failed: ${e.message}`);
      this.scanning = false;
      if (this.scanCallback) {
        this.scanCallback([]);
      }
      return;
    }

    // 启动扫描 — 参考 04/06/07 phone 样例，使用单个 deviceName 过滤
    // 部分机型会把多 filter 当 AND 处理导致扫不到；APP 应连接 Hub: XH_HOME
    try {
      const scanFilter: scan.ScanFilters = {
        deviceName: HUB_DEVICE_NAME,
      };
      const scanOptions: scan.ScanOptions = {
        scanMode: SCAN_MODE,
      };
      scan.startScan([scanFilter], scanOptions);
      console.info(`${TAG} scan started (filter=${HUB_DEVICE_NAME}, mode=${SCAN_MODE})`);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} startScan failed: code=${e.code} msg=${e.message}`);
      this.scanning = false;
      if (this.scanCallback) {
        this.scanCallback([]);
      }
      return;
    }

    // 兜底：如果精确过滤 4s 没结果，切到无过滤扫描。
    // 这样可兼容旧固件把 C 字符串结尾 \0 放进广播名导致底层过滤不命中的情况。
    this.scanFallbackTimer = setTimeout((): void => {
      this.switchToUnfilteredScanIfEmpty();
    }, SCAN_FILTER_FALLBACK_MS);

    // 设置扫描超时（参考示例 20s）
    this.scanTimer = setTimeout((): void => {
      console.info(`${TAG} scan timeout`);
      this.stopScan();
    }, SCAN_TIMEOUT);
  }

  /**
   * 停止扫描
   */
  stopScan(): void {
    if (!this.scanning && this.scanTimer === -1) {
      return;
    }

    this.scanning = false;

    if (this.scanTimer !== -1) {
      clearTimeout(this.scanTimer);
      this.scanTimer = -1;
    }
    if (this.scanFallbackTimer !== -1) {
      clearTimeout(this.scanFallbackTimer);
      this.scanFallbackTimer = -1;
    }

    try {
      scan.stopScan();
      scan.off('deviceFound');
      console.info(`${TAG} scan stopped`);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} stopScan failed: ${e.message}`);
    }
  }

  /** 是否正在扫描 */
  isScanning(): boolean {
    return this.scanning;
  }

  /** 扫描结果内部处理 */
  private onScanResultInternal(results: scan.ScanResults[]): void {
    for (let i: number = 0; i < results.length; i++) {
      const result: scan.ScanResults = results[i];
      const name: string = this.normalizeDeviceName(result.deviceName);

      // 过滤空名称（参考示例: result.deviceName != null && length > 0）
      if (!name || name.length === 0) {
        continue;
      }

      // 只展示 APP 可连接的目标设备：
      // 1. XH_HOME：Hub 广播名，APP 正常应连接它
      // 2. XH_TH_FAN：参考头文件默认名，防止固件未使用 BUILD.gn 宏覆盖
      // 3. XH_M_*：保留兼容直连模组调试场景
      if (!this.isTargetDeviceName(name)) {
        continue;
      }

      // 使用 result.rssi（示例确认该字段可用）
      const rssi: number = result.rssi;
      const device: DeviceInfo = makeDeviceInfo(name, result.address, rssi);
      this.addOrUpdateDevice(device);
    }

    if (this.foundDevices.length > 0 && this.scanFallbackTimer !== -1) {
      clearTimeout(this.scanFallbackTimer);
      this.scanFallbackTimer = -1;
    }

    // 通知 UI
    if (this.scanCallback) {
      this.scanCallback(this.foundDevices.slice());
    }
  }

  /** 精确过滤没有结果时，切换到无过滤扫描，避免广播名格式问题导致回调进不来 */
  private switchToUnfilteredScanIfEmpty(): void {
    this.scanFallbackTimer = -1;
    if (!this.scanning || this.foundDevices.length > 0) {
      return;
    }
    try {
      scan.stopScan();
    } catch (err) {
      // 忽略停止失败，继续尝试启动无过滤扫描
    }
    try {
      const scanOptions: scan.ScanOptions = {
        scanMode: SCAN_MODE,
      };
      scan.startScan([], scanOptions);
      console.info(`${TAG} fallback scan started (no filter, mode=${SCAN_MODE})`);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} fallback startScan failed: code=${e.code} msg=${e.message}`);
    }
  }

  /** 添加或更新已发现设备（去重） */
  private addOrUpdateDevice(device: DeviceInfo): void {
    for (let i: number = 0; i < this.foundDevices.length; i++) {
      if (this.foundDevices[i].mac === device.mac) {
        // 更新 rssi（参考示例: foundDevice.rssi = bleDevice.rssi）
        this.foundDevices[i] = device;
        return;
      }
    }
    this.foundDevices.push(device);
  }

  /** 判断扫描到的名称是否为本 APP 需要展示的设备 */
  private isTargetDeviceName(name: string): boolean {
    return name === HUB_DEVICE_NAME ||
      name === LEGACY_HUB_DEVICE_NAME ||
      name.startsWith('XH_M_');
  }

  /** 设备端广播名可能带 C 字符串结尾的 \0，这里先规整后再匹配 */
  private normalizeDeviceName(name: string): string {
    if (!name) {
      return '';
    }
    const zeroIndex: number = name.indexOf('\u0000');
    if (zeroIndex >= 0) {
      return name.substring(0, zeroIndex).trim();
    }
    return name.trim();
  }

  // ================================================================
  //  连接（对应 PPT 步骤 5，参考示例 connectToDevice）
  // ================================================================

  /**
   * 连接到星闪设备
   * 参考示例流程:
   *   1. await ssap.createClient(mac)
   *   2. client.on('connectionStateChange') — 在 STATE_CONNECTED 时注册 propertyChange + getServices
   *   3. client.connect()
   *
   * @param mac 设备地址（来自扫描结果）
   * @param callback 连接结果回调
   * @param name 设备名称（用于解析模组 ID）
   */
  async connect(mac: string, callback: (connected: boolean) => void, name?: string): Promise<void> {
    if (this.connState !== ConnState.IDLE) {
      console.info(`${TAG} already connecting/connected`);
      callback(false);
      return;
    }

    this.connState = ConnState.CONNECTING;
    this.connectedMac = mac;
    this.connectedName = name ?? 'XH_HOME';
    this.connectedModuleId = parseModuleIdFromName(this.connectedName);
    this.connectCallback = callback;

    console.info(
      `${TAG} connecting to ${mac} (name=${this.connectedName}, moduleId=${this.connectedModuleId})`,
    );

    // 1. 创建 SSAP 客户端（参考示例: await ssap.createClient）
    try {
      this.client = await ssap.createClient(mac);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} createClient failed: ${e.message}`);
      this.connState = ConnState.IDLE;
      callback(false);
      return;
    }

    if (!this.client) {
      console.error(`${TAG} createClient returned null`);
      this.connState = ConnState.IDLE;
      callback(false);
      return;
    }

    // 2. 订阅连接状态变化（参考示例: 在 STATE_CONNECTED 时注册 propertyChange + getServices）
    try {
      this.client.on('connectionStateChange', (state: ssap.ConnectionChangeState): void => {
        console.info(`${TAG} connectionStateChange: state=${state.state}`);

        if (state.state === constant.ConnectionState.STATE_CONNECTED) {
          // 连接成功 — 异步发现服务和订阅
          this.onConnected();
        } else if (state.state === constant.ConnectionState.STATE_DISCONNECTED) {
          // 断开连接
          this.onDisconnected();
        }
      });
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} client.on(connectionStateChange) failed: ${e.message}`);
    }

    // 3. 发起连接（参考示例: connectedDevice?.connect()）
    try {
      this.client.connect();
      console.info(`${TAG} connect() called`);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} connect() failed: code=${e.code} msg=${e.message}`);
      this.connState = ConnState.IDLE;
      this.cleanupClient();
      callback(false);
    }
  }

  /**
   * 连接成功后的处理（参考示例: 在 connectionStateChange STATE_CONNECTED 分支中执行）
   * 1. 注册 propertyChange 回调
   * 2. getServices() 发现服务
   * 3. 找到目标属性并订阅 Notify
   * 4. 启动读取轮询
   */
  private async onConnected(): Promise<void> {
    if (!this.client) {
      return;
    }

    console.info(`${TAG} connected, registering propertyChange...`);

    // 1. 注册属性变化回调（参考示例: 在 STATE_CONNECTED 时注册）
    try {
      this.client.on('propertyChange', this.propertyChangeCb);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} client.on(propertyChange) failed: ${e.message}`);
    }

    // 2. 发现服务（参考示例: await client.getServices()）
    try {
      this.serviceList = await this.client.getServices();
      console.info(`${TAG} found ${this.serviceList.length} services`);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} getServices failed: ${e.message}`);
      this.connState = ConnState.IDLE;
      this.cleanupClient();
      if (this.connectCallback) {
        this.connectCallback(false);
      }
      return;
    }

    // 通知 UI 服务列表
    if (this.servicesCallback) {
      this.servicesCallback(this.serviceList);
    }

    // 3. 先标记已连接，避免订阅 Notify 后立即上报的数据被 propertyChangeCb 过滤
    this.connState = ConnState.CONNECTED;

    // 4. 查找目标属性并订阅 Notify
    this.discoverAndSubscribe();

    // 5. 启动主动读取和心跳触发，兼容设备端不主动 Notify 的情况
    this.startReadPolling();
    this.startHeartbeatPolling();

    // 6. 通知 UI 连接成功
    if (this.connectCallback) {
      this.connectCallback(true);
    }
  }

  /**
   * 断开连接的处理（参考示例: 在 STATE_DISCONNECTED 分支中 client.close()）
   */
  private onDisconnected(): void {
    console.info(`${TAG} disconnected`);
    this.stopReadPolling();
    this.stopHeartbeatPolling();
    this.connState = ConnState.IDLE;

    if (this.connectCallback) {
      this.connectCallback(false);
    }

    // 参考示例: client.close() 取消订阅所有事件
    this.cleanupClient();
  }

  /**
   * 发现服务并订阅 Notify
   * 参考示例: 遍历 services，按 operation 标志位找到可通知属性
   *   operation & 0x01 = 可读
   *   operation & 0x04 = 可写
   *   operation & 0x08 = 可通知
   */
  private discoverAndSubscribe(): void {
    if (!this.client) {
      return;
    }

    let foundByUuid: boolean = false;

    // 方式1: 按 UUID 精确查找（02 固件使用 0x2222/0x2323）
    for (let i: number = 0; i < this.serviceList.length; i++) {
      const svc: ssap.Service = this.serviceList[i];
      console.info(`${TAG} service: ${svc.serviceUuid}`);

      if (this.uuidEquals(svc.serviceUuid, XH_SERVICE_UUID) && svc.properties) {
        for (let j: number = 0; j < svc.properties.length; j++) {
          const prop: ssap.Property = svc.properties[j];
          console.info(`${TAG}   property: ${prop.propertyUuid} op=${prop.operation}`);

          if (this.uuidEquals(prop.propertyUuid, XH_PROPERTY_UUID)) {
            this.targetProperty = prop;
            this.targetServiceUuid = svc.serviceUuid;
            this.targetPropertyUuid = prop.propertyUuid;
            foundByUuid = true;
            console.info(`${TAG} found target by UUID: ${prop.propertyUuid}`);
            break;
          }
        }
        if (foundByUuid) {
          break;
        }
      }
    }

    // 方式2: 按 operation 标志位查找可通知属性（参考示例，作为回退）
    if (!foundByUuid) {
      console.info(`${TAG} target not found by UUID, trying by operation flag...`);
      for (let i: number = 0; i < this.serviceList.length; i++) {
        const svc: ssap.Service = this.serviceList[i];
        if (svc.properties) {
          for (let j: number = 0; j < svc.properties.length; j++) {
            const prop: ssap.Property = svc.properties[j];
            // operation & 0x08 = 可通知
            if (prop.operation !== undefined && (prop.operation & 0x08) !== 0) {
              this.targetProperty = prop;
              this.targetServiceUuid = svc.serviceUuid;
              this.targetPropertyUuid = prop.propertyUuid;
              foundByUuid = true;
              console.info(`${TAG} found notifiable property by operation: ${prop.propertyUuid}`);
              break;
            }
          }
        }
        if (foundByUuid) {
          break;
        }
      }
    }

    if (!this.targetProperty) {
      console.error(`${TAG} no suitable property found`);
      return;
    }

    // 订阅属性变化 (Notify) — 参考示例: client.setPropertyNotification(property, true)
    try {
      this.client.setPropertyNotification(this.targetProperty, true);
      console.info(`${TAG} notify subscribed`);
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} setPropertyNotification failed: ${e.message}`);
    }
  }

  // ================================================================
  //  数据订阅（对应 PPT 步骤 6-9）
  // ================================================================

  /**
   * 订阅传感器数据
   * @param mac 设备地址（兼容旧接口，实际使用已连接设备）
   * @param callback 传感器数据回调
   */
  subscribeSensorData(mac: string, callback: (data: SensorData) => void): void {
    this.sensorCallback = callback;
    if (this.latestSensorData) {
      try {
        callback(this.latestSensorData);
      } catch (err) {
        console.error(`${TAG} sensor callback failed`);
      }
    }
  }

  /**
   * 设置原始数据回调（用于 SSAP 调试显示）
   */
  setRawDataCallback(callback: (data: Uint8Array) => void): void {
    this.rawCallback = callback;
  }

  /**
   * 设置服务列表回调（用于 SSAP 调试显示）
   */
  setServicesCallback(callback: (services: ssap.Service[]) => void): void {
    this.servicesCallback = callback;
  }

  /** 停止传感器数据订阅 */
  stopSensorSubscription(): void {
    this.sensorCallback = null;
    this.rawCallback = null;
    this.stopReadPolling();
    this.stopHeartbeatPolling();
  }

  /**
   * 处理收到的数据（Notify 或 Read）
   */
  private handleReceivedData(data: ArrayBuffer): void {
    if (!data || data.byteLength === 0) {
      return;
    }

    const bytes: Uint8Array = arrayBufferToUint8(data);

    // 调用原始数据回调
    if (this.rawCallback) {
      try {
        this.rawCallback(bytes);
      } catch (err) {
        console.error(`${TAG} raw callback failed`);
      }
    }

    // 解析温湿度数据
    const thData: ThData | null = parseThPayload(bytes);
    if (thData && this.sensorCallback) {
      const sensorData: SensorData = {
        temperature: thData.temp100 / 100.0,
        humidity: thData.humi100 / 100.0,
        temp100: thData.temp100,
        humi100: thData.humi100,
        presence: thData.presence,
        fanLevel: thData.fanLevel,
        lightMode: thData.lightMode,
        sceneMode: thData.sceneMode,
        hasTempHumi: thData.hasTempHumi,
        hasPresence: thData.hasPresence,
        hasFan: thData.hasFan,
        hasLight: thData.hasLight,
        hasScene: thData.hasScene,
      };
      this.latestSensorData = sensorData;
      console.info(
        `${TAG} sensor: temp=${sensorData.temperature.toFixed(2)}°C humi=${sensorData.humidity.toFixed(2)}% presence=${sensorData.presence} hasTempHumi=${sensorData.hasTempHumi} hasPresence=${sensorData.hasPresence}`,
      );
      try {
        this.sensorCallback(sensorData);
      } catch (err) {
        console.error(`${TAG} sensor callback failed`);
      }
    }
  }

  // ================================================================
  //  读取轮询（对应 PPT 步骤 8，参考示例 pollTimer 500ms）
  // ================================================================

  /** 启动读取轮询 */
  private startReadPolling(): void {
    this.stopReadPolling();
    this.readTimer = setInterval((): void => {
      this.doRead().catch((err: BusinessError): void => {
        // 读取失败时静默处理
      });
    }, READ_INTERVAL);
    console.info(`${TAG} read polling started (${READ_INTERVAL}ms)`);
  }

  /** 停止读取轮询 */
  private stopReadPolling(): void {
    if (this.readTimer !== -1) {
      clearInterval(this.readTimer);
      this.readTimer = -1;
      console.info(`${TAG} read polling stopped`);
    }
  }

  /** 启动心跳轮询，定期触发 SHT30 上报温湿度 */
  private startHeartbeatPolling(): void {
    this.stopHeartbeatPolling();
    this.heartbeatTimer = setInterval((): void => {
      this.sendHeartbeat();
    }, HEARTBEAT_INTERVAL);
    console.info(`${TAG} heartbeat polling started (${HEARTBEAT_INTERVAL}ms)`);
  }

  /** 停止心跳轮询 */
  private stopHeartbeatPolling(): void {
    if (this.heartbeatTimer !== -1) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = -1;
      console.info(`${TAG} heartbeat polling stopped`);
    }
  }

  /** 执行一次读取 */
  private async doRead(): Promise<void> {
    if (!this.client || this.connState !== ConnState.CONNECTED) {
      return;
    }
    if (!this.targetProperty) {
      return;
    }

    try {
      const result: ssap.Property = await this.client.readProperty(this.targetProperty);
      if (result && result.value) {
        this.handleReceivedData(result.value);
      }
    } catch (err) {
      const e: BusinessError = err as BusinessError;
      console.error(`${TAG} read failed: ${e.message}`);
    }
  }

  // ================================================================
  //  控制命令（对应设备端 XH CONTROL 消息）
  // ================================================================

  /**
   * 发送控制命令
   * 参考示例 writeProperty: 找到完整 property → 设 value → writeProperty(WRITE)
   * @param mac 设备地址（兼容旧接口）
   * @param command 控制命令
   * @returns true 如果命令已发送
   */
  sendControl(mac: string, command: ControlCommand): boolean {
    if (!this.client || this.connState !== ConnState.CONNECTED) {
      console.error(`${TAG} not connected, cannot send control`);
      return false;
    }
    if (!this.targetProperty) {
      console.error(`${TAG} no target property, cannot send control`);
      return false;
    }

    // 确定目标模组 ID 和 TLV 列表
    let targetModuleId: number = 0;
    let tlvs: XhTlv[] = [];

    const isOn: boolean = command.mode !== 0;

    switch (command.type) {
      case ControlType.FAN: {
        targetModuleId = XhModuleId.FAN;
        // FAN_LEVEL: value 33→1, 66→2, 100→3
        let fanLevel: number = 0;
        if (isOn) {
          fanLevel = command.value <= 33 ? 1 : command.value <= 66 ? 2 : 3;
        }
        tlvs = [
          tlvU8(XhTlvType.SWITCH, isOn ? 1 : 0),
          tlvU8(XhTlvType.FAN_LEVEL, fanLevel),
        ];
        console.info(`${TAG} FAN control: on=${isOn} level=${fanLevel}`);
        break;
      }

      case ControlType.LIGHT: {
        targetModuleId = XhModuleId.LIGHT;
        // LIGHT_MODE: value ≤33→dim(3), 34-66→warm(1), 67-100→cold(2)
        let lightMode: number = 0;
        if (isOn) {
          lightMode = command.value <= 33 ? 3 : command.value <= 66 ? 1 : 2;
        }
        tlvs = [
          tlvU8(XhTlvType.SWITCH, isOn ? 1 : 0),
          tlvU8(XhTlvType.LIGHT_MODE, lightMode),
        ];
        console.info(`${TAG} LIGHT control: on=${isOn} mode=${lightMode}`);
        break;
      }

      default:
        console.error(`${TAG} unknown control type: ${command.type}`);
        return false;
    }

    // 检查连接的设备是否匹配目标模组
    if (this.connectedModuleId !== targetModuleId && this.connectedModuleId !== 0) {
      console.info(
        `${TAG} connected module (${this.connectedModuleId}) != target (${targetModuleId}), command may be ignored`,
      );
    }

    // 编码 XH CONTROL 帧
    const frame: Uint8Array = encodeControl(targetModuleId, tlvs);

    try {
      // 参考示例: 设 property.value 然后调用 writeProperty
      this.targetProperty.value = uint8ToArrayBuffer(frame);

      this.client
        .writeProperty(this.targetProperty, ssap.PropertyWriteType.WRITE)
        .then((): void => {
          console.info(`${TAG} control sent: ${bytesToHex(frame)}`);
        })
        .catch((err: BusinessError): void => {
          console.error(`${TAG} writeProperty failed: code=${err.code} msg=${err.message}`);
        });
    } catch (err) {
      console.error(`${TAG} writeProperty sync failed`);
      return false;
    }

    return true;
  }

  /**
   * 发送 Hub 场景模式控制
   * @param mode 1=居家, 2=离家, 3=睡眠
   */
  sendSceneMode(mode: number): boolean {
    if (!this.client || this.connState !== ConnState.CONNECTED) {
      console.error(`${TAG} not connected, cannot send scene`);
      return false;
    }
    if (!this.targetProperty) {
      console.error(`${TAG} no target property, cannot send scene`);
      return false;
    }

    const frame: Uint8Array = encodeSceneControl(mode);
    try {
      this.targetProperty.value = uint8ToArrayBuffer(frame);

      this.client
        .writeProperty(this.targetProperty, ssap.PropertyWriteType.WRITE)
        .then((): void => {
          console.info(`${TAG} scene sent: mode=${mode} frame=${bytesToHex(frame)}`);
        })
        .catch((err: BusinessError): void => {
          console.error(`${TAG} scene writeProperty failed: code=${err.code} msg=${err.message}`);
        });
    } catch (err) {
      console.error(`${TAG} scene writeProperty sync failed`);
      return false;
    }

    return true;
  }

  /**
   * 发送 HEARTBEAT 消息（触发 SHT30 立即上报温湿度）
   * @returns true 如果已发送
   */
  sendHeartbeat(): boolean {
    if (!this.client || this.connState !== ConnState.CONNECTED) {
      console.error(`${TAG} not connected, cannot send heartbeat`);
      return false;
    }
    if (!this.targetProperty) {
      console.error(`${TAG} no target property, cannot send heartbeat`);
      return false;
    }

    const moduleId: number = this.connectedModuleId || XhModuleId.SHT30;
    const frame: Uint8Array = encodeHeartbeat(moduleId);

    this.targetProperty.value = uint8ToArrayBuffer(frame);

    this.client
      .writeProperty(this.targetProperty, ssap.PropertyWriteType.WRITE)
      .then((): void => {
        console.info(`${TAG} heartbeat sent: ${bytesToHex(frame)}`);
      })
      .catch((err: BusinessError): void => {
        console.error(`${TAG} heartbeat failed: ${err.message}`);
      });

    return true;
  }

  // ================================================================
  //  断开连接
  // ================================================================

  /** 断开连接 */
  disconnect(): void {
    console.info(`${TAG} disconnecting...`);
    this.stopReadPolling();
    this.stopHeartbeatPolling();
    this.connState = ConnState.IDLE;
    this.sensorCallback = null;
    this.rawCallback = null;
    this.connectCallback = null;
    this.servicesCallback = null;
    this.latestSensorData = null;

    if (this.client) {
      try {
        this.client.disconnect();
      } catch (err) {
        const e: BusinessError = err as BusinessError;
        console.error(`${TAG} disconnect failed: ${e.message}`);
      }
    }

    // 参考示例: client.close() 取消所有事件订阅
    this.cleanupClient();
  }

  /** 清理客户端资源 */
  private cleanupClient(): void {
    if (this.client) {
      try {
        // 参考示例: close() 取消所有事件订阅
        this.client.close();
      } catch (err) {
        const e: BusinessError = err as BusinessError;
        console.error(`${TAG} client.close() failed: ${e.message}`);
      }
      this.client = null;
    }
    this.serviceList = [];
    this.targetProperty = null;
    this.targetServiceUuid = '';
    this.targetPropertyUuid = '';
    this.connectedMac = '';
    this.connectedName = '';
    this.connectedModuleId = 0;
  }

  // ================================================================
  //  状态查询
  // ================================================================

  /** 是否已连接 */
  isConnected(): boolean {
    return this.connState === ConnState.CONNECTED;
  }

  /** 获取已连接设备名称 */
  getConnectedName(): string {
    return this.connectedName;
  }

  /** 获取已连接设备模组 ID */
  getConnectedModuleId(): number {
    return this.connectedModuleId;
  }

  /** 获取服务列表（用于 SSAP 调试显示） */
  getServiceList(): ssap.Service[] {
    return this.serviceList;
  }

  // ================================================================
  //  工具函数
  // ================================================================

  /** UUID 大小写不敏感比较 */
  private uuidEquals(a: string, b: string): boolean {
    return a.toLowerCase() === b.toLowerCase();
  }
}

// ===== 格式化辅助函数 =====

/** 格式化温度 (temp100 → "25.63") */
export function formatTemp(temp100: number): string {
  return (temp100 / 100.0).toFixed(2);
}

/** 格式化湿度 (humi100 → "58.20") */
export function formatHumi(humi100: number): string {
  return (humi100 / 100.0).toFixed(2);
}

// ===== 导出单例 =====

export const sleHelper: SleHelper = new SleHelper();

/**
 * 设备数据模型
 *
 * 描述扫描发现的星闪设备及设备类型映射。
 * Hub 设备名: XH_HOME
 * 模组设备名格式: XH_M_01 (SHT30), XH_M_03 (Fan), XH_M_07 (Light)
 */

import { XhModuleId } from '../sle/XhProto';

// ===== 设备类型 =====

export enum DeviceType {
  HUB = 'HUB',
  SENSOR = 'SENSOR',
  LIGHT = 'LIGHT',
  FAN = 'FAN',
  UNKNOWN = 'UNKNOWN',
}

// ===== 风扇档位 =====

export enum FanSpeed {
  OFF = 0,
  LOW = 1,
  MEDIUM = 2,
  HIGH = 3,
}

// ===== 灯光模式（对应设备端 LIGHT_MODE）=====

export enum LightMode {
  OFF = 0,
  WARM = 1,
  COLD = 2,
  DIM = 3,
}

// ===== 场景模式 =====

export enum SceneMode {
  HOME = 'HOME',
  AWAY = 'AWAY',
  SLEEP = 'SLEEP',
}

// ===== 控制命令 =====

export enum ControlType {
  FAN = 'FAN',
  LIGHT = 'LIGHT',
}

export interface ControlCommand {
  type: ControlType;
  mode: number; // 0=off, 1=on
  value: number; // 档位/亮度/模式值
}

// ===== 传感器数据 =====

export interface SensorData {
  temperature: number; // °C
  humidity: number;    // %
  temp100: number;     // 原始值 (温度×100)
  humi100: number;     // 原始值 (湿度×100)
  presence: number;    // -1=暂无数据, 0=不在, 1=人在
  hasTempHumi: boolean;
  hasPresence: boolean;
  fanLevel: number;
  lightMode: number;
  sceneMode: number;
  hasFan: boolean;
  hasLight: boolean;
  hasScene: boolean;
}

// ===== 设备信息 =====

export interface DeviceInfo {
  name: string;
  mac: string;
  rssi: number;
  type: DeviceType;
  description: string;
}

// ===== 模块名与类型映射 =====

/**
 * 根据设备广播名解析模组 ID
 * XH_M_01 → 1, XH_M_03 → 3, XH_M_07 → 7
 * XH_HOME 是 Hub，不等同于某一个具体模组
 */
export function parseModuleIdFromName(name: string): number {
  if (name === 'XH_HOME' || name === 'XH_TH_FAN') {
    return 0;
  }
  if (name.startsWith('XH_M_')) {
    const idStr: string = name.substring(5); // "01", "03", "07"
    const id: number = parseInt(idStr, 10);
    if (!isNaN(id)) {
      return id;
    }
  }
  return 0; // 未知
}

/**
 * 根据模组 ID 获取设备类型
 */
export function deviceTypeFromModuleId(moduleId: number): DeviceType {
  switch (moduleId) {
    case XhModuleId.SHT30:
    case XhModuleId.BH1750:
    case XhModuleId.LD2401:
      return DeviceType.SENSOR;
    case XhModuleId.FAN:
      return DeviceType.FAN;
    case XhModuleId.LIGHT:
      return DeviceType.LIGHT;
    default:
      return DeviceType.UNKNOWN;
  }
}

/**
 * 根据设备广播名获取设备类型
 */
export function deviceTypeFromName(name: string): DeviceType {
  if (name === 'XH_HOME' || name === 'XH_TH_FAN') {
    return DeviceType.HUB;
  }
  const moduleId: number = parseModuleIdFromName(name);
  return deviceTypeFromModuleId(moduleId);
}

/**
 * 根据设备广播名获取中文描述
 */
export function deviceDescriptionFromName(name: string): string {
  if (name === 'XH_HOME' || name === 'XH_TH_FAN') {
    return '智能家居 Hub';
  }
  const moduleId: number = parseModuleIdFromName(name);
  switch (moduleId) {
    case XhModuleId.SHT30:
      return '温湿度传感器';
    case XhModuleId.FAN:
      return '风扇模组';
    case XhModuleId.BH1750:
      return '光照传感器';
    case XhModuleId.LD2401:
      return '人体存在传感器';
    case XhModuleId.LIGHT:
      return '灯光模组';
    default:
      return '未知设备';
  }
}

/**
 * 构造 DeviceInfo 对象
 */
export function makeDeviceInfo(name: string, mac: string, rssi: number): DeviceInfo {
  return {
    name: name,
    mac: mac,
    rssi: rssi,
    type: deviceTypeFromName(name),
    description: deviceDescriptionFromName(name),
  };
}

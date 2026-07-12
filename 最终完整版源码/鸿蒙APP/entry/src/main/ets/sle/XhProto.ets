/**
 * XH 星闪协议编解码器
 *
 * 对应设备端 proto/xh_sle_proto.c / xh_sle_proto.h
 *
 * 帧格式（小端序）:
 *   Byte 0: 'X' (0x58)
 *   Byte 1: 'H' (0x48)
 *   Byte 2: version (1)
 *   Byte 3: seq
 *   Byte 4: module_id
 *   Byte 5: msg_type
 *   Byte 6: tlv_len (后续 TLV 总字节数)
 *   Byte 7..: TLV 数据 (type:1 + len:1 + value:len)
 */

// ===== 协议常量 =====

/** 魔数 */
export const XH_MAGIC_0: number = 0x58; // 'X'
export const XH_MAGIC_1: number = 0x48; // 'H'
/** 协议版本 */
export const XH_VERSION: number = 1;
/** 帧头长度 */
export const XH_HDR_LEN: number = 7;
/** 最大帧长度 */
export const XH_MAX_LEN: number = 255;

// ===== 模组 ID（对应 xh_module_ids.h）=====

export class XhModuleId {
  static readonly HUB_SCENE: number = 0;
  static readonly SHT30: number = 1;
  static readonly FAN: number = 3;
  static readonly BH1750: number = 4;
  static readonly LD2401: number = 5;
  static readonly LIGHT: number = 7;
}

// ===== 消息类型（对应 xh_sle_proto.h XH_PROTO_MSG_*）=====

export class XhMsgType {
  static readonly HELLO: number = 0x01;
  static readonly REPORT: number = 0x02;
  static readonly CONTROL: number = 0x03;
  static readonly ACK: number = 0x04;
  static readonly HEARTBEAT: number = 0x05;
}

// ===== TLV 类型（对应 xh_sle_proto.h XH_TLV_*）=====

export class XhTlvType {
  static readonly ONLINE: number = 0x01;
  static readonly VALID: number = 0x02;
  static readonly WARMUP: number = 0x03;
  static readonly TEMP100: number = 0x10;
  static readonly HUMI100: number = 0x11;
  static readonly LUX100: number = 0x14;
  static readonly PRESENCE: number = 0x15;
  static readonly SWITCH: number = 0x20;
  static readonly FAN_LEVEL: number = 0x21;
  static readonly LIGHT_MODE: number = 0x27;
  static readonly GPIO_OUTPUT: number = 0x28;
  static readonly ERROR_CODE: number = 0x30;
  static readonly AGE_MS: number = 0x31;
  static readonly SCENE_MODE: number = 0x40;
  static readonly SCENE_CAUSE: number = 0x41;
  static readonly SCENE_FLAGS: number = 0x42;
}

// ===== ACK 状态码 =====

export class XhAck {
  static readonly OK: number = 0;
  static readonly INVALID: number = 1;
}

// ===== SSAP UUID（16-bit → 128-bit，NearLink 基址 UUID）=====

/** NearLink 基址 UUID 前缀 */
const NEARLINK_UUID_BASE: string = '37bea880-fc70-11ea-b720-00000000';

/**
 * 将 16-bit UUID 转换为 NearLink 128-bit UUID
 * 0x2222 → '37bea880-fc70-11ea-b720-000000002222'
 */
export function uuid16To128(uuid16: number): string {
  const hex: string = uuid16.toString(16).padStart(4, '0');
  return NEARLINK_UUID_BASE + hex;
}

/** 服务 UUID: 0x2222 */
export const XH_SERVICE_UUID: string = uuid16To128(0x2222);
/** 属性 UUID: 0x2323 (READ | WRITE | NOTIFY) */
export const XH_PROPERTY_UUID: string = uuid16To128(0x2323);
/** CCCD UUID: 0x2902 */
export const XH_CCCD_UUID: string = uuid16To128(0x2902);

// ===== 数据结构 =====

/** TLV 条目 */
export interface XhTlv {
  type: number;
  data: Uint8Array;
}

/** 解码后的 XH 消息 */
export interface XhMessage {
  valid: boolean;
  version: number;
  seq: number;
  moduleId: number;
  msgType: number;
  tlvs: XhTlv[];
}

// ===== 序列号管理 =====

let g_seq: number = 0;

/** 获取下一个序列号（0~255 循环） */
export function xhNextSeq(): number {
  g_seq = (g_seq + 1) & 0xFF;
  return g_seq;
}

// ===== TLV 构造辅助 =====

/** 构造 u8 TLV */
export function tlvU8(type: number, value: number): XhTlv {
  const data: Uint8Array = new Uint8Array(1);
  data[0] = value & 0xFF;
  return { type, data };
}

/** 构造 i32 TLV（小端序） */
export function tlvI32(type: number, value: number): XhTlv {
  const data: Uint8Array = new Uint8Array(4);
  const buf: ArrayBuffer = new ArrayBuffer(4);
  const view: DataView = new DataView(buf);
  view.setInt32(0, value, true); // 小端序
  data[0] = view.getUint8(0);
  data[1] = view.getUint8(1);
  data[2] = view.getUint8(2);
  data[3] = view.getUint8(3);
  return { type, data };
}

// ===== TLV 读取辅助 =====

/** 从消息中读取 u8 TLV 值 */
export function getTlvU8(msg: XhMessage, type: number): number | null {
  for (let i: number = 0; i < msg.tlvs.length; i++) {
    const tlv: XhTlv = msg.tlvs[i];
    if (tlv.type === type && tlv.data.length >= 1) {
      return tlv.data[0];
    }
  }
  return null;
}

/** 从消息中读取 i32 TLV 值（小端序） */
export function getTlvI32(msg: XhMessage, type: number): number | null {
  for (let i: number = 0; i < msg.tlvs.length; i++) {
    const tlv: XhTlv = msg.tlvs[i];
    if (tlv.type === type && tlv.data.length >= 4) {
      const buf: ArrayBuffer = new ArrayBuffer(4);
      const view: DataView = new DataView(buf);
      view.setUint8(0, tlv.data[0]);
      view.setUint8(1, tlv.data[1]);
      view.setUint8(2, tlv.data[2]);
      view.setUint8(3, tlv.data[3]);
      return view.getInt32(0, true); // 小端序
    }
  }
  return null;
}

// ===== 帧编码 =====

/**
 * 编码 XH 协议帧
 * @param moduleId 目标模组 ID
 * @param msgType 消息类型 (XhMsgType)
 * @param tlvs TLV 列表（可选，HEARTBEAT 无需 TLV）
 * @returns 编码后的 Uint8Array
 */
export function encodeXhFrame(moduleId: number, msgType: number, tlvs?: XhTlv[]): Uint8Array {
  // 计算 TLV 总长度
  let tlvLen: number = 0;
  if (tlvs) {
    for (let i: number = 0; i < tlvs.length; i++) {
      tlvLen += 2 + tlvs[i].data.length; // type(1) + len(1) + value
    }
  }

  // 限制 TLV 长度
  if (tlvLen > 255) {
    tlvLen = 255;
  }

  // 创建帧缓冲区
  const frame: Uint8Array = new Uint8Array(XH_HDR_LEN + tlvLen);

  // 写入帧头
  frame[0] = XH_MAGIC_0; // 'X'
  frame[1] = XH_MAGIC_1; // 'H'
  frame[2] = XH_VERSION;
  frame[3] = xhNextSeq();
  frame[4] = moduleId & 0xFF;
  frame[5] = msgType & 0xFF;
  frame[6] = tlvLen & 0xFF;

  // 写入 TLV
  let offset: number = XH_HDR_LEN;
  if (tlvs) {
    for (let i: number = 0; i < tlvs.length && offset < frame.length; i++) {
      const tlv: XhTlv = tlvs[i];
      frame[offset++] = tlv.type & 0xFF;
      const tlvDataLen: number = Math.min(tlv.data.length, 255);
      frame[offset++] = tlvDataLen & 0xFF;
      for (let j: number = 0; j < tlvDataLen && offset < frame.length; j++) {
        frame[offset++] = tlv.data[j];
      }
    }
  }

  return frame;
}

/**
 * 编码 CONTROL 消息（风扇/灯光控制）
 * @param moduleId 目标模组 ID
 * @param tlvs 控制参数 TLV 列表
 * @returns 编码后的 Uint8Array
 */
export function encodeControl(moduleId: number, tlvs: XhTlv[]): Uint8Array {
  return encodeXhFrame(moduleId, XhMsgType.CONTROL, tlvs);
}

/**
 * 编码场景控制消息
 * mode: 1=居家, 2=离家, 3=睡眠
 */
export function encodeSceneControl(mode: number): Uint8Array {
  return encodeControl(XhModuleId.HUB_SCENE, [
    tlvU8(XhTlvType.SCENE_MODE, mode),
  ]);
}

/**
 * 编码 HEARTBEAT 消息（触发 SHT30 上报）
 * @param moduleId 目标模组 ID
 * @returns 编码后的 Uint8Array
 */
export function encodeHeartbeat(moduleId: number): Uint8Array {
  return encodeXhFrame(moduleId, XhMsgType.HEARTBEAT);
}

// ===== 帧解码 =====

/**
 * 解码 XH 协议帧
 * @param data 原始字节数据
 * @returns 解码后的消息（valid=false 表示解析失败）
 */
export function decodeXhFrame(data: Uint8Array): XhMessage {
  const msg: XhMessage = {
    valid: false,
    version: 0,
    seq: 0,
    moduleId: 0,
    msgType: 0,
    tlvs: []
  };

  // 检查最小长度
  if (data.length < XH_HDR_LEN) {
    return msg;
  }

  // 检查魔数
  if (data[0] !== XH_MAGIC_0 || data[1] !== XH_MAGIC_1) {
    return msg;
  }

  msg.version = data[2];
  msg.seq = data[3];
  msg.moduleId = data[4];
  msg.msgType = data[5];
  const tlvLen: number = data[6];

  // 检查数据完整性
  if (data.length < XH_HDR_LEN + tlvLen) {
    return msg;
  }

  // 解析 TLV
  let offset: number = XH_HDR_LEN;
  let remaining: number = tlvLen;
  while (remaining >= 2) {
    const type: number = data[offset++];
    const len: number = data[offset++];
    if (remaining < 2 + len) {
      break;
    }
    const value: Uint8Array = data.slice(offset, offset + len);
    msg.tlvs.push({ type, data: value });
    offset += len;
    remaining -= 2 + len;
  }

  msg.valid = true;
  return msg;
}

// ===== 数据解析 =====

/** 温湿度数据解析结果 */
export interface ThData {
  temp100: number;
  humi100: number;
  presence: number;
  fanLevel: number;
  lightMode: number;
  sceneMode: number;
  hasTempHumi: boolean;
  hasPresence: boolean;
  hasFan: boolean;
  hasLight: boolean;
  hasScene: boolean;
}

/**
 * 从 XH REPORT 消息中解析传感器数据
 * @param data 原始字节数据
 * @returns 传感器数据，解析失败返回 null
 */
export function parseThFromXh(data: Uint8Array): ThData | null {
  const msg: XhMessage = decodeXhFrame(data);
  if (!msg.valid) {
    return null;
  }

  const temp100: number | null = getTlvI32(msg, XhTlvType.TEMP100);
  const humi100: number | null = getTlvI32(msg, XhTlvType.HUMI100);
  const presence: number | null = getTlvU8(msg, XhTlvType.PRESENCE);
  const sw: number | null = getTlvU8(msg, XhTlvType.SWITCH);
  const fanLevel: number | null = getTlvU8(msg, XhTlvType.FAN_LEVEL);
  const lightMode: number | null = getTlvU8(msg, XhTlvType.LIGHT_MODE);
  const sceneMode: number | null = getTlvU8(msg, XhTlvType.SCENE_MODE);

  const hasFan: boolean = msg.moduleId === XhModuleId.FAN && (fanLevel !== null || sw !== null);
  const hasLight: boolean = msg.moduleId === XhModuleId.LIGHT && (lightMode !== null || sw !== null);
  const hasScene: boolean = msg.moduleId === XhModuleId.HUB_SCENE && sceneMode !== null;

  if ((temp100 === null || humi100 === null) && presence === null &&
    !hasFan && !hasLight && !hasScene) {
    return null;
  }

  return {
    temp100: temp100 === null ? 0 : temp100,
    humi100: humi100 === null ? 0 : humi100,
    presence: presence === null ? -1 : presence,
    fanLevel: fanLevel === null ? (sw === null || sw === 0 ? 0 : 1) : fanLevel,
    lightMode: lightMode === null ? (sw === null || sw === 0 ? 0 : 1) : lightMode,
    sceneMode: sceneMode === null ? 0 : sceneMode,
    hasTempHumi: temp100 !== null && humi100 !== null,
    hasPresence: presence !== null,
    hasFan,
    hasLight,
    hasScene,
  };
}

/**
 * 解析 Legacy 8 字节温湿度数据
 * 对应 PPT: 字节 0~3 为温度×100 (i32 LE)，字节 4~7 为湿度×100 (i32 LE)
 * @param data 至少 8 字节数据
 * @returns 温湿度数据，解析失败返回 null
 */
export function parseLegacyTh(data: Uint8Array): ThData | null {
  if (data.length !== 8) {
    return null;
  }
  const buf: ArrayBuffer = new ArrayBuffer(8);
  const view: DataView = new DataView(buf);
  for (let i: number = 0; i < 8; i++) {
    view.setUint8(i, data[i]);
  }
  const temp100: number = view.getInt32(0, true);
  const humi100: number = view.getInt32(4, true);
  return {
    temp100,
    humi100,
    presence: -1,
    fanLevel: 0,
    lightMode: 0,
    sceneMode: 0,
    hasTempHumi: true,
    hasPresence: false,
    hasFan: false,
    hasLight: false,
    hasScene: false,
  };
}

/**
 * 智能解析温湿度数据（自动判断 XH 协议或 Legacy 格式）
 * @param data 原始字节数据
 * @returns 温湿度数据，解析失败返回 null
 */
export function parseThPayload(data: Uint8Array): ThData | null {
  // 先尝试 XH 协议格式
  if (data.length >= XH_HDR_LEN && data[0] === XH_MAGIC_0 && data[1] === XH_MAGIC_1) {
    return parseThFromXh(data);
  }
  // 回退到 Legacy 8 字节格式
  return parseLegacyTh(data);
}

// ===== 工具函数 =====

/**
 * Uint8Array 转 ArrayBuffer
 */
export function uint8ToArrayBuffer(arr: Uint8Array): ArrayBuffer {
  const buffer: ArrayBuffer = new ArrayBuffer(arr.length);
  const view: Uint8Array = new Uint8Array(buffer);
  view.set(arr);
  return buffer;
}

/**
 * ArrayBuffer 转 Uint8Array
 */
export function arrayBufferToUint8(buffer: ArrayBuffer): Uint8Array {
  return new Uint8Array(buffer);
}

/**
 * 将字节数组转换为十六进制字符串（调试用）
 */
export function bytesToHex(data: Uint8Array, maxLen: number = 64): string {
  const len: number = Math.min(data.length, maxLen);
  let hex: string = '';
  for (let i: number = 0; i < len; i++) {
    hex += data[i].toString(16).padStart(2, '0') + ' ';
  }
  if (data.length > maxLen) {
    hex += '...';
  }
  return hex.trim().toUpperCase();
}

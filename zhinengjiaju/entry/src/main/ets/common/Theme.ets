/**
 * 全局主题常量
 */

/** 主色调 */
export class AppColors {
  /* 背景 */
  static readonly BG_PAGE: string = '#F2F4F8';
  static readonly BG_CARD: string = '#FFFFFF';
  static readonly BG_CARD_DARK: string = '#F8F9FB';

  /* 主题色 */
  static readonly PRIMARY: string = '#3B82F6';
  static readonly PRIMARY_DARK: string = '#2563EB';
  static readonly ACCENT: string = '#6366F1';

  /* 文字 */
  static readonly TEXT_PRIMARY: string = '#1F2937';
  static readonly TEXT_SECONDARY: string = '#6B7280';
  static readonly TEXT_HINT: string = '#9CA3AF';
  static readonly TEXT_WHITE: string = '#FFFFFF';

  /* 渐变 - 页面头部 */
  static readonly GRADIENT_HEADER_START: string = '#4F6BED';
  static readonly GRADIENT_HEADER_END: string = '#7B3FF2';

  /* 渐变 - 温度卡片 */
  static readonly GRADIENT_TEMP_START: string = '#FF8A65';
  static readonly GRADIENT_TEMP_END: string = '#FF5722';

  /* 渐变 - 湿度卡片 */
  static readonly GRADIENT_HUMI_START: string = '#4FC3F7';
  static readonly GRADIENT_HUMI_END: string = '#0288D1';

  /* 渐变 - 风扇卡片 */
  static readonly GRADIENT_FAN_START: string = '#4DD0E1';
  static readonly GRADIENT_FAN_END: string = '#00BCD4';

  /* 渐变 - 灯光卡片 */
  static readonly GRADIENT_LIGHT_START: string = '#FFD54F';
  static readonly GRADIENT_LIGHT_END: string = '#FFC107';

  /* 状态色 */
  static readonly SUCCESS: string = '#22C55E';
  static readonly WARNING: string = '#F59E0B';
  static readonly DANGER: string = '#EF4444';
  static readonly INFO: string = '#3B82F6';

  /* 边框/分割 */
  static readonly BORDER: string = '#E5E7EB';
  static readonly DIVIDER: string = '#F0F1F3';
}

/** 圆角 */
export class AppRadius {
  static readonly SM: number = 8;
  static readonly MD: number = 12;
  static readonly LG: number = 16;
  static readonly XL: number = 20;
  static readonly FULL: number = 9999;
}

/** 间距 */
export class AppSpacing {
  static readonly XS: number = 4;
  static readonly SM: number = 8;
  static readonly MD: number = 12;
  static readonly LG: number = 16;
  static readonly XL: number = 20;
  static readonly XXL: number = 24;
  static readonly XXXL: number = 32;
}

/** 字号 */
export class AppFontSize {
  static readonly XS: number = 10;
  static readonly SM: number = 12;
  static readonly MD: number = 14;
  static readonly LG: number = 16;
  static readonly XL: number = 18;
  static readonly XXL: number = 22;
  static readonly XXXL: number = 28;
  static readonly HUGE: number = 36;
  static readonly MASSIVE: number = 48;
}

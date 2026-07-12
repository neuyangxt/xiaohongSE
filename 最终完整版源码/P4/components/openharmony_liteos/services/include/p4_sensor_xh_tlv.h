#ifndef P4_SENSOR_XH_TLV_H
#define P4_SENSOR_XH_TLV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define P4_XH_MODULE_HUB_SCENE  0x00U
#define P4_XH_MODULE_SHT30      0x01U
#define P4_XH_MODULE_MQ2        0x02U
#define P4_XH_MODULE_FAN        0x03U
#define P4_XH_MODULE_BH1750     0x04U
#define P4_XH_MODULE_PRESENCE   0x05U
#define P4_XH_MODULE_ALARM      0x06U
#define P4_XH_MODULE_LIGHT      0x07U

#define P4_XH_MSG_REPORT        0x02U
#define P4_XH_MSG_CONTROL       0x03U
#define P4_XH_MSG_ACK           0x04U

#define P4_XH_TLV_ONLINE             0x01U
#define P4_XH_TLV_VALID              0x02U
#define P4_XH_TLV_WARMUP             0x03U
#define P4_XH_TLV_TEMP100            0x10U
#define P4_XH_TLV_HUMI100            0x11U
#define P4_XH_TLV_SMOKE_ALARM        0x12U
#define P4_XH_TLV_RAW_VALUE          0x13U
#define P4_XH_TLV_LUX100             0x14U
#define P4_XH_TLV_PRESENCE           0x15U
#define P4_XH_TLV_ADC_VALID          0x16U
#define P4_XH_TLV_ADC_RAW            0x17U
#define P4_XH_TLV_ADC_AVG            0x18U
#define P4_XH_TLV_SWITCH             0x20U
#define P4_XH_TLV_FAN_LEVEL          0x21U
#define P4_XH_TLV_ALARM_MODE         0x22U
#define P4_XH_TLV_TIMEOUT_PROTECTED  0x23U
#define P4_XH_TLV_RGB                0x26U
#define P4_XH_TLV_GPIO_OUTPUT        0x28U
#define P4_XH_TLV_ERROR_CODE         0x30U
#define P4_XH_TLV_AGE_MS             0x31U
#define P4_XH_TLV_ADC_ERROR          0x32U
#define P4_XH_TLV_SCENE_MODE         0x40U
#define P4_XH_TLV_SCENE_CAUSE        0x41U
#define P4_XH_TLV_SCENE_FLAGS        0x42U

typedef struct {
    uint8_t version;
    uint8_t seq;
    uint8_t module_id;
    uint8_t msg;

    uint8_t online_valid;
    uint8_t online;
    uint8_t valid_valid;
    uint8_t valid;
    uint8_t warmup_valid;
    uint8_t warmup;

    uint8_t temp_valid;
    int32_t temp100;
    uint8_t humi_valid;
    int32_t humi100;
    uint8_t smoke_alarm_valid;
    uint8_t smoke_alarm;
    uint8_t raw_value_valid;
    uint32_t raw_value;
    uint8_t lux_valid;
    uint32_t lux100;
    uint8_t presence_valid;
    uint8_t presence;
    uint8_t adc_valid_valid;
    uint8_t adc_valid;
    uint8_t adc_raw_valid;
    uint32_t adc_raw;
    uint8_t adc_avg_valid;
    uint32_t adc_avg;

    uint8_t switch_valid;
    uint8_t switch_on;
    uint8_t fan_level_valid;
    uint8_t fan_level;
    uint8_t alarm_mode_valid;
    uint8_t alarm_mode;
    uint8_t timeout_protected_valid;
    uint8_t timeout_protected;
    uint8_t rgb_valid;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;
    uint8_t gpio_output_valid;
    uint8_t gpio_output;
    uint8_t error_valid;
    uint16_t error_code;
    uint8_t age_ms_valid;
    int32_t age_ms;
    uint8_t adc_error_valid;
    uint16_t adc_error;
    uint8_t scene_mode_valid;
    uint8_t scene_mode;
    uint8_t scene_cause_valid;
    uint8_t scene_cause;
    uint8_t scene_flags_valid;
    uint8_t scene_flags;
} P4XhTlvFrame;

int P4XhTlvIsFrame(const uint8_t *payload, uint16_t len);
int P4XhTlvParse(const uint8_t *payload, uint16_t len, P4XhTlvFrame *out);
int P4XhTlvBuildFanControl(uint8_t seq,
                           uint8_t on,
                           uint8_t level,
                           uint8_t *out,
                           uint16_t cap,
                           uint16_t *outLen);
int P4XhTlvBuildAlarmControl(uint8_t seq,
                             uint8_t mode,
                             uint8_t *out,
                             uint16_t cap,
                             uint16_t *outLen);
int P4XhTlvBuildLightControl(uint8_t seq,
                             uint8_t on,
                             uint8_t r,
                             uint8_t g,
                             uint8_t b,
                             uint8_t brightness,
                             uint8_t *out,
                             uint16_t cap,
                             uint16_t *outLen);
int P4XhTlvBuildSceneControl(uint8_t seq,
                             uint8_t mode,
                             uint8_t *out,
                             uint16_t cap,
                             uint16_t *outLen);
const char *P4XhTlvAckErrorName(uint16_t errorCode);

#ifdef __cplusplus
}
#endif

#endif

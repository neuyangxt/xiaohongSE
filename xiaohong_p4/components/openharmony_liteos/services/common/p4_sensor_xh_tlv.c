#include "p4_sensor_xh_tlv.h"

#include <string.h>

#define P4_XH_HEADER_LEN 7U
#define P4_XH_VERSION    0x01U

static uint16_t P4XhReadLe16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t P4XhReadLe32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t P4XhReadLe32Signed(const uint8_t *p)
{
    return (int32_t)P4XhReadLe32(p);
}

static int P4XhPutTlvU8(uint8_t *out, uint16_t cap, uint16_t *pos, uint8_t type, uint8_t value)
{
    if (out == NULL || pos == NULL || *pos + 3U > cap) {
        return -1;
    }
    out[(*pos)++] = type;
    out[(*pos)++] = 1U;
    out[(*pos)++] = value;
    return 0;
}

static int P4XhPutTlvRgb(uint8_t *out,
                         uint16_t cap,
                         uint16_t *pos,
                         uint8_t r,
                         uint8_t g,
                         uint8_t b,
                         uint8_t brightness)
{
    if (out == NULL || pos == NULL || *pos + 6U > cap) {
        return -1;
    }
    out[(*pos)++] = P4_XH_TLV_RGB;
    out[(*pos)++] = 4U;
    out[(*pos)++] = r;
    out[(*pos)++] = g;
    out[(*pos)++] = b;
    out[(*pos)++] = brightness;
    return 0;
}

static int P4XhBegin(uint8_t seq, uint8_t moduleId, uint8_t msg, uint8_t *out, uint16_t cap, uint16_t *pos)
{
    if (out == NULL || pos == NULL || cap < P4_XH_HEADER_LEN) {
        return -1;
    }
    out[0] = 0x58U;
    out[1] = 0x48U;
    out[2] = P4_XH_VERSION;
    out[3] = seq;
    out[4] = moduleId;
    out[5] = msg;
    out[6] = 0U;
    *pos = P4_XH_HEADER_LEN;
    return 0;
}

static int P4XhFinish(uint8_t *out, uint16_t pos, uint16_t *outLen)
{
    if (out == NULL || outLen == NULL || pos < P4_XH_HEADER_LEN || pos - P4_XH_HEADER_LEN > 255U) {
        return -1;
    }
    out[6] = (uint8_t)(pos - P4_XH_HEADER_LEN);
    *outLen = pos;
    return 0;
}

int P4XhTlvIsFrame(const uint8_t *payload, uint16_t len)
{
    return (payload != NULL &&
            len >= P4_XH_HEADER_LEN &&
            payload[0] == 0x58U &&
            payload[1] == 0x48U &&
            payload[2] == P4_XH_VERSION);
}

int P4XhTlvParse(const uint8_t *payload, uint16_t len, P4XhTlvFrame *out)
{
    uint16_t pos;
    uint16_t end;

    if (!P4XhTlvIsFrame(payload, len) || out == NULL) {
        return -1;
    }

    (void)memset(out, 0, sizeof(*out));
    out->version = payload[2];
    out->seq = payload[3];
    out->module_id = payload[4];
    out->msg = payload[5];
    end = (uint16_t)(P4_XH_HEADER_LEN + payload[6]);
    if (end > len) {
        return -2;
    }

    pos = P4_XH_HEADER_LEN;
    while (pos < end) {
        uint8_t type;
        uint8_t tlvLen;
        const uint8_t *v;

        if (pos + 2U > end) {
            return -3;
        }
        type = payload[pos++];
        tlvLen = payload[pos++];
        if (pos + tlvLen > end) {
            return -4;
        }
        v = &payload[pos];

        switch (type) {
            case P4_XH_TLV_ONLINE:
                if (tlvLen != 1U) { return -5; }
                out->online_valid = 1U;
                out->online = v[0];
                break;
            case P4_XH_TLV_VALID:
                if (tlvLen != 1U) { return -5; }
                out->valid_valid = 1U;
                out->valid = v[0];
                break;
            case P4_XH_TLV_WARMUP:
                if (tlvLen != 1U) { return -5; }
                out->warmup_valid = 1U;
                out->warmup = v[0];
                break;
            case P4_XH_TLV_TEMP100:
                if (tlvLen != 4U) { return -5; }
                out->temp_valid = 1U;
                out->temp100 = P4XhReadLe32Signed(v);
                break;
            case P4_XH_TLV_HUMI100:
                if (tlvLen != 4U) { return -5; }
                out->humi_valid = 1U;
                out->humi100 = P4XhReadLe32Signed(v);
                break;
            case P4_XH_TLV_SMOKE_ALARM:
                if (tlvLen != 1U) { return -5; }
                out->smoke_alarm_valid = 1U;
                out->smoke_alarm = v[0];
                break;
            case P4_XH_TLV_RAW_VALUE:
                if (tlvLen != 4U) { return -5; }
                out->raw_value_valid = 1U;
                out->raw_value = P4XhReadLe32(v);
                break;
            case P4_XH_TLV_LUX100:
                if (tlvLen != 4U) { return -5; }
                out->lux_valid = 1U;
                out->lux100 = P4XhReadLe32(v);
                break;
            case P4_XH_TLV_PRESENCE:
                if (tlvLen != 1U) { return -5; }
                out->presence_valid = 1U;
                out->presence = v[0];
                break;
            case P4_XH_TLV_ADC_VALID:
                if (tlvLen != 1U) { return -5; }
                out->adc_valid_valid = 1U;
                out->adc_valid = v[0];
                break;
            case P4_XH_TLV_ADC_RAW:
                if (tlvLen != 4U) { return -5; }
                out->adc_raw_valid = 1U;
                out->adc_raw = P4XhReadLe32(v);
                break;
            case P4_XH_TLV_ADC_AVG:
                if (tlvLen != 4U) { return -5; }
                out->adc_avg_valid = 1U;
                out->adc_avg = P4XhReadLe32(v);
                break;
            case P4_XH_TLV_SWITCH:
                if (tlvLen != 1U) { return -5; }
                out->switch_valid = 1U;
                out->switch_on = v[0];
                break;
            case P4_XH_TLV_FAN_LEVEL:
                if (tlvLen != 1U) { return -5; }
                out->fan_level_valid = 1U;
                out->fan_level = v[0];
                break;
            case P4_XH_TLV_ALARM_MODE:
                if (tlvLen != 1U) { return -5; }
                out->alarm_mode_valid = 1U;
                out->alarm_mode = v[0];
                break;
            case P4_XH_TLV_TIMEOUT_PROTECTED:
                if (tlvLen != 1U) { return -5; }
                out->timeout_protected_valid = 1U;
                out->timeout_protected = v[0];
                break;
            case P4_XH_TLV_RGB:
                if (tlvLen != 4U) { return -5; }
                out->rgb_valid = 1U;
                out->r = v[0];
                out->g = v[1];
                out->b = v[2];
                out->brightness = v[3];
                break;
            case P4_XH_TLV_GPIO_OUTPUT:
                if (tlvLen != 1U) { return -5; }
                out->gpio_output_valid = 1U;
                out->gpio_output = v[0];
                break;
            case P4_XH_TLV_ERROR_CODE:
                if (tlvLen != 2U) { return -5; }
                out->error_valid = 1U;
                out->error_code = P4XhReadLe16(v);
                break;
            case P4_XH_TLV_AGE_MS:
                if (tlvLen != 4U) { return -5; }
                out->age_ms_valid = 1U;
                out->age_ms = P4XhReadLe32Signed(v);
                break;
            case P4_XH_TLV_ADC_ERROR:
                if (tlvLen != 2U) { return -5; }
                out->adc_error_valid = 1U;
                out->adc_error = P4XhReadLe16(v);
                break;
            case P4_XH_TLV_SCENE_MODE:
                if (tlvLen != 1U) { return -5; }
                out->scene_mode_valid = 1U;
                out->scene_mode = v[0];
                break;
            case P4_XH_TLV_SCENE_CAUSE:
                if (tlvLen != 1U) { return -5; }
                out->scene_cause_valid = 1U;
                out->scene_cause = v[0];
                break;
            case P4_XH_TLV_SCENE_FLAGS:
                if (tlvLen != 1U) { return -5; }
                out->scene_flags_valid = 1U;
                out->scene_flags = v[0];
                break;
            default:
                break;
        }
        pos = (uint16_t)(pos + tlvLen);
    }

    return 0;
}

int P4XhTlvBuildFanControl(uint8_t seq,
                           uint8_t on,
                           uint8_t level,
                           uint8_t *out,
                           uint16_t cap,
                           uint16_t *outLen)
{
    uint16_t pos;

    if (P4XhBegin(seq, P4_XH_MODULE_FAN, P4_XH_MSG_CONTROL, out, cap, &pos) != 0) {
        return -1;
    }
    if (P4XhPutTlvU8(out, cap, &pos, P4_XH_TLV_SWITCH, on ? 1U : 0U) != 0 ||
        P4XhPutTlvU8(out, cap, &pos, P4_XH_TLV_FAN_LEVEL, level) != 0) {
        return -1;
    }
    return P4XhFinish(out, pos, outLen);
}

int P4XhTlvBuildAlarmControl(uint8_t seq,
                             uint8_t mode,
                             uint8_t *out,
                             uint16_t cap,
                             uint16_t *outLen)
{
    uint16_t pos;
    uint8_t on = (mode != 0U) ? 1U : 0U;

    if (P4XhBegin(seq, P4_XH_MODULE_ALARM, P4_XH_MSG_CONTROL, out, cap, &pos) != 0) {
        return -1;
    }
    if (P4XhPutTlvU8(out, cap, &pos, P4_XH_TLV_SWITCH, on) != 0) {
        return -1;
    }
    if (on && P4XhPutTlvU8(out, cap, &pos, P4_XH_TLV_ALARM_MODE, mode) != 0) {
        return -1;
    }
    return P4XhFinish(out, pos, outLen);
}

int P4XhTlvBuildLightControl(uint8_t seq,
                             uint8_t on,
                             uint8_t r,
                             uint8_t g,
                             uint8_t b,
                             uint8_t brightness,
                             uint8_t *out,
                             uint16_t cap,
                             uint16_t *outLen)
{
    uint16_t pos;

    if (P4XhBegin(seq, P4_XH_MODULE_LIGHT, P4_XH_MSG_CONTROL, out, cap, &pos) != 0) {
        return -1;
    }
    if (!on) {
        r = 0U;
        g = 0U;
        b = 0U;
        brightness = 0U;
    }
    if (P4XhPutTlvU8(out, cap, &pos, P4_XH_TLV_SWITCH, on ? 1U : 0U) != 0 ||
        P4XhPutTlvRgb(out, cap, &pos, r, g, b, brightness) != 0) {
        return -1;
    }
    return P4XhFinish(out, pos, outLen);
}

int P4XhTlvBuildSceneControl(uint8_t seq,
                             uint8_t mode,
                             uint8_t *out,
                             uint16_t cap,
                             uint16_t *outLen)
{
    uint16_t pos;

    if (mode != 1U && mode != 2U) {
        return -1;
    }
    if (P4XhBegin(seq, P4_XH_MODULE_HUB_SCENE, P4_XH_MSG_CONTROL, out, cap, &pos) != 0) {
        return -1;
    }
    if (P4XhPutTlvU8(out, cap, &pos, P4_XH_TLV_SCENE_MODE, mode) != 0) {
        return -1;
    }
    return P4XhFinish(out, pos, outLen);
}

const char *P4XhTlvAckErrorName(uint16_t errorCode)
{
    switch (errorCode) {
        case 0U: return "OK";
        case 1U: return "INVALID";
        case 2U: return "UNSUPPORTED";
        case 3U: return "OFFLINE";
        case 4U: return "SEND_FAIL";
        default: return "UNKNOWN";
    }
}

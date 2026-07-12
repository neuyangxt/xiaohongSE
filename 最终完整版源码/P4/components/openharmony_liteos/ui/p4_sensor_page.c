#include "p4_sensor_page.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "ohos_uart_link_ui.h"
#include "p4_status_bar.h"
#include "p4_system_font.h"
#include "p4_ui_model.h"

static const char *TAG = "p4_sensor";

typedef enum {
    P4_SENSOR_TAB_ENV = 0,
    P4_SENSOR_TAB_CONTROL,
} P4SensorTab;

typedef struct {
    lv_obj_t *status;
    lv_obj_t *title;
    lv_obj_t *tab_env;
    lv_obj_t *tab_control;
    lv_obj_t *summary;
    lv_obj_t *line1;
    lv_obj_t *line2;
    lv_obj_t *line3;
    lv_obj_t *line4;
    lv_obj_t *ack1;
    lv_obj_t *ack2;
    lv_obj_t *ack3;
    lv_obj_t *scene;
    lv_obj_t *scene_ack;
    lv_obj_t *back;
} P4SensorPageUi;

static P4SensorPageBackHandler s_back_handler;
static P4SensorPageUi s_ui;
static uint32_t s_visible;
static P4SensorTab s_tab;
static uint32_t s_have_fingerprint;
static uint32_t s_last_fingerprint;
static uint32_t s_last_control_ms;
static uint32_t s_last_press_action_ms;
static uint32_t s_last_periodic_refresh_ms;
static char s_last_action[72];

static void P4SensorBuildPage(void);

#ifndef P4_SENSOR_PERIODIC_REFRESH_MS
#define P4_SENSOR_PERIODIC_REFRESH_MS 1000U
#endif

static void P4SensorHashAdd(uint32_t *hash, uint32_t value)
{
    if (hash == NULL) {
        return;
    }
    *hash ^= value + 0x9e3779b9U + (*hash << 6) + (*hash >> 2);
}

static uint32_t P4SensorSnapshotFingerprint(const OhosUartLinkSensorSnapshot *s)
{
    uint32_t h = 0x811c9dc5U;

    if (s == NULL) {
        return 0U;
    }

    P4SensorHashAdd(&h, s->valid);
    P4SensorHashAdd(&h, (uint32_t)s->temp100);
    P4SensorHashAdd(&h, (uint32_t)s->humi100);
    P4SensorHashAdd(&h, s->sht30_online_valid);
    P4SensorHashAdd(&h, s->sht30_online);
    P4SensorHashAdd(&h, s->fan_online_valid);
    P4SensorHashAdd(&h, s->fan_online);
    P4SensorHashAdd(&h, s->fan_switch_valid);
    P4SensorHashAdd(&h, s->fan_switch_on);
    P4SensorHashAdd(&h, s->fan_level_valid);
    P4SensorHashAdd(&h, s->fan_level);
    P4SensorHashAdd(&h, s->fan_control_tx_count);
    P4SensorHashAdd(&h, s->fan_control_ack_count);
    P4SensorHashAdd(&h, s->fan_control_ack_error);
    P4SensorHashAdd(&h, s->alarm_online_valid);
    P4SensorHashAdd(&h, s->alarm_online);
    P4SensorHashAdd(&h, s->alarm_mode_valid);
    P4SensorHashAdd(&h, s->alarm_mode);
    P4SensorHashAdd(&h, s->alarm_control_tx_count);
    P4SensorHashAdd(&h, s->alarm_control_ack_count);
    P4SensorHashAdd(&h, s->alarm_control_ack_error);
    P4SensorHashAdd(&h, s->light_online_valid);
    P4SensorHashAdd(&h, s->light_online);
    P4SensorHashAdd(&h, s->light_switch_valid);
    P4SensorHashAdd(&h, s->light_switch_on);
    P4SensorHashAdd(&h, s->light_rgb_valid);
    P4SensorHashAdd(&h, s->light_r);
    P4SensorHashAdd(&h, s->light_g);
    P4SensorHashAdd(&h, s->light_b);
    P4SensorHashAdd(&h, s->light_brightness);
    P4SensorHashAdd(&h, s->light_gpio_output_valid);
    P4SensorHashAdd(&h, s->light_gpio_output);
    P4SensorHashAdd(&h, s->light_control_tx_count);
    P4SensorHashAdd(&h, s->light_control_ack_count);
    P4SensorHashAdd(&h, s->light_control_last_tx_ok);
    P4SensorHashAdd(&h, s->light_control_tx_seq);
    P4SensorHashAdd(&h, s->light_control_tx_ms);
    P4SensorHashAdd(&h, s->light_control_desired_on);
    P4SensorHashAdd(&h, s->light_control_desired_r);
    P4SensorHashAdd(&h, s->light_control_desired_g);
    P4SensorHashAdd(&h, s->light_control_desired_b);
    P4SensorHashAdd(&h, s->light_control_desired_brightness);
    P4SensorHashAdd(&h, s->light_control_ack_seq);
    P4SensorHashAdd(&h, s->light_control_ack_error);
    P4SensorHashAdd(&h, s->light_control_ack_last_ms);
    P4SensorHashAdd(&h, s->scene_mode_valid);
    P4SensorHashAdd(&h, s->scene_mode);
    P4SensorHashAdd(&h, s->scene_cause_valid);
    P4SensorHashAdd(&h, s->scene_cause);
    P4SensorHashAdd(&h, s->scene_flags_valid);
    P4SensorHashAdd(&h, s->scene_flags);
    P4SensorHashAdd(&h, s->scene_report_count);
    P4SensorHashAdd(&h, s->scene_control_tx_count);
    P4SensorHashAdd(&h, s->scene_control_last_tx_ok);
    P4SensorHashAdd(&h, s->scene_control_tx_seq);
    P4SensorHashAdd(&h, s->scene_control_tx_ms);
    P4SensorHashAdd(&h, s->scene_control_desired_mode);
    P4SensorHashAdd(&h, s->scene_control_ack_count);
    P4SensorHashAdd(&h, s->scene_control_ack_seq);
    P4SensorHashAdd(&h, s->scene_control_ack_error);
    P4SensorHashAdd(&h, (uint32_t)s_tab);
    return h;
}

static lv_obj_t *P4SensorCreateLabel(lv_obj_t *parent,
                                     const char *text,
                                     int x,
                                     int y,
                                     int w,
                                     int h,
                                     lv_color_t color,
                                     lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, P4SystemFontGet(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_label_set_text(label, text);
    return label;
}

static void P4SensorSetLabel(lv_obj_t *label, const char *text)
{
    const char *oldText;

    if (label == NULL || text == NULL || !lv_obj_is_valid(label)) {
        return;
    }
    oldText = lv_label_get_text(label);
    if (oldText != NULL && strcmp(oldText, text) == 0) {
        return;
    }
    lv_label_set_text(label, text);
}

static lv_obj_t *P4SensorCreateRect(lv_obj_t *parent,
                                    int x,
                                    int y,
                                    int w,
                                    int h,
                                    lv_color_t color,
                                    int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, lv_color_make(0xD9, 0xE2, 0xEC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *P4SensorCreateButton(lv_obj_t *parent,
                                      const char *text,
                                      int x,
                                      int y,
                                      int w,
                                      int h,
                                      lv_color_t color,
                                      lv_event_cb_t cb,
                                      void *userData)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *label;

    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, userData);

    label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, P4SystemFontGet(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static void P4SensorFormatFixed100(char *buf, size_t len, int32_t value100, const char *unit)
{
    const char *sign = "";

    if (buf == NULL || len == 0U) {
        return;
    }
    if (value100 < 0) {
        sign = "-";
        value100 = -value100;
    }
    (void)snprintf(buf,
                   len,
                   "%s%d.%02d %s",
                   sign,
                   (int)(value100 / 100),
                   (int)(value100 % 100),
                   unit ? unit : "");
}

static const char *P4SensorOnlineText(uint32_t valid, uint32_t online)
{
    if (!valid) {
        return "--";
    }
    return online ? "在线" : "离线";
}

static const char *P4SensorOnOffText(uint32_t valid, uint32_t on)
{
    if (!valid) {
        return "--";
    }
    return on ? "开" : "关";
}

static const char *P4SensorAlarmModeText(uint32_t valid, uint32_t mode)
{
    if (!valid) {
        return "--";
    }
    switch (mode) {
        case 0U: return "关";
        case 1U: return "常亮";
        case 2U: return "闪烁";
        case 3U: return "蜂鸣";
        case 4U: return "组合";
        case 5U: return "短鸣";
        default: return "未知";
    }
}

static const char *P4SensorAckText(uint32_t txCount, uint32_t txOk, uint32_t ackCount, uint32_t error)
{
    if (txCount == 0U) {
        return "未控制";
    }
    if (txOk < txCount && ackCount == 0U) {
        return "发送失败";
    }
    if (ackCount == 0U) {
        return "发送中";
    }
    switch (error) {
        case 0U: return "ACK成功";
        case 1U: return "参数错误";
        case 2U: return "不支持";
        case 3U: return "模块离线";
        case 4U: return "发送失败";
        default: return "控制失败";
    }
}

static const char *P4SensorSceneModeText(uint32_t valid, uint32_t mode)
{
    if (!valid) {
        return "--";
    }
    switch (mode) {
        case 1U: return "居家";
        case 2U: return "离家";
        default: return "未知";
    }
}

static void P4SensorFormatSceneCause(char *buf, size_t len, uint32_t valid, uint32_t cause)
{
    char tmp[72];
    size_t used = 0U;

    if (buf == NULL || len == 0U) {
        return;
    }
    if (!valid || cause == 0U) {
        (void)snprintf(buf, len, "状态 正常");
        return;
    }

    tmp[0] = '\0';
#define P4_SENSOR_APPEND_CAUSE(bit, text)                                       \
    do {                                                                        \
        if ((cause & (bit)) != 0U && used < sizeof(tmp)) {                      \
            int written = snprintf(&tmp[used], sizeof(tmp) - used, "%s%s",      \
                                   used == 0U ? "" : " ", (text));             \
            if (written > 0) {                                                  \
                used += (size_t)written;                                        \
                if (used >= sizeof(tmp)) {                                      \
                    used = sizeof(tmp) - 1U;                                    \
                }                                                               \
            }                                                                   \
        }                                                                       \
    } while (0)

    P4_SENSOR_APPEND_CAUSE(0x01U, "过热");
    P4_SENSOR_APPEND_CAUSE(0x02U, "过湿");
    P4_SENSOR_APPEND_CAUSE(0x04U, "烟雾");
    P4_SENSOR_APPEND_CAUSE(0x08U, "有人");
    P4_SENSOR_APPEND_CAUSE(0x10U, "失败");
#undef P4_SENSOR_APPEND_CAUSE

    (void)snprintf(buf, len, "状态 %s", tmp[0] ? tmp : "未知");
}

static void P4SensorFormatSceneCommand(char *buf, size_t len, const OhosUartLinkSensorSnapshot *s)
{
    uint32_t nowMs;

    if (buf == NULL || len == 0U) {
        return;
    }
    if (s == NULL || s->scene_control_tx_count == 0U) {
        (void)snprintf(buf, len, "未控制");
        return;
    }
    if (!s->scene_control_last_tx_ok) {
        (void)snprintf(buf, len, "发送失败");
        return;
    }
    if (s->scene_control_ack_count == 0U ||
        s->scene_control_ack_seq != s->scene_control_tx_seq) {
        (void)snprintf(buf, len, "发送中");
        return;
    }
    if (s->scene_control_ack_error != 0U) {
        (void)snprintf(buf,
                       len,
                       "%s",
                       P4SensorAckText(s->scene_control_tx_count,
                                       s->scene_control_tx_ok,
                                       s->scene_control_ack_count,
                                       s->scene_control_ack_error));
        return;
    }
    if (s->scene_mode_valid && s->scene_mode == s->scene_control_desired_mode &&
        s->scene_last_ms >= s->scene_control_tx_ms) {
        (void)snprintf(buf, len, "已生效");
        return;
    }

    nowMs = (uint32_t)esp_log_timestamp();
    if (s->scene_control_ack_last_ms != 0U &&
        (uint32_t)(nowMs - s->scene_control_ack_last_ms) > 2000U) {
        (void)snprintf(buf, len, "ACK成功 等上报");
    } else {
        (void)snprintf(buf, len, "ACK成功 等待");
    }
}

static uint32_t P4SensorLightReportMatchesDesired(const OhosUartLinkSensorSnapshot *s)
{
    if (s == NULL || s->light_control_tx_count == 0U || !s->light_control_last_tx_ok) {
        return 0U;
    }
    if (s->light_control_ack_count == 0U ||
        s->light_control_ack_seq != s->light_control_tx_seq ||
        s->light_control_ack_error != 0U) {
        return 0U;
    }
    if (s->light_control_tx_ms != 0U && s->light_last_ms != 0U &&
        s->light_last_ms < s->light_control_tx_ms) {
        return 0U;
    }
    if (!s->light_switch_valid || s->light_switch_on != s->light_control_desired_on) {
        return 0U;
    }
    if (!s->light_control_desired_on) {
        return 1U;
    }
    if (!s->light_rgb_valid) {
        return 0U;
    }
    return (s->light_r == s->light_control_desired_r &&
            s->light_g == s->light_control_desired_g &&
            s->light_b == s->light_control_desired_b &&
            s->light_brightness == s->light_control_desired_brightness) ? 1U : 0U;
}

static void P4SensorFormatLightCommand(char *buf, size_t len, const OhosUartLinkSensorSnapshot *s)
{
    uint32_t nowMs;

    if (buf == NULL || len == 0U) {
        return;
    }
    if (s == NULL || s->light_control_tx_count == 0U) {
        (void)snprintf(buf, len, "未控制");
        return;
    }
    if (!s->light_control_last_tx_ok) {
        (void)snprintf(buf, len, "发送失败");
        return;
    }
    if (s->light_control_ack_count == 0U ||
        s->light_control_ack_seq != s->light_control_tx_seq) {
        (void)snprintf(buf, len, "发送中");
        return;
    }
    if (s->light_control_ack_error != 0U) {
        (void)snprintf(buf,
                       len,
                       "%s",
                       P4SensorAckText(s->light_control_tx_count,
                                       s->light_control_tx_ok,
                                       s->light_control_ack_count,
                                       s->light_control_ack_error));
        return;
    }
    if (P4SensorLightReportMatchesDesired(s)) {
        (void)snprintf(buf, len, "已生效");
        return;
    }

    nowMs = (uint32_t)esp_log_timestamp();
    if (s->light_control_ack_last_ms != 0U &&
        (uint32_t)(nowMs - s->light_control_ack_last_ms) > 2000U) {
        (void)snprintf(buf, len, "ACK成功 状态未变");
    } else {
        (void)snprintf(buf, len, "ACK成功 等待上报");
    }
}

static void P4SensorLogTouchLatency(const char *id, uint32_t cbStartMs, uint32_t cbEndMs)
{
    ESP_LOGI(TAG,
             "[P4-TOUCH-LAT] page=sensor id=%s cb_ms=%u",
             id ? id : "unknown",
             (unsigned)(cbEndMs - cbStartMs));
}

static uint32_t P4SensorControlDebounced(const char *id, uint32_t startMs)
{
    if (s_last_control_ms != 0U && (uint32_t)(startMs - s_last_control_ms) < 250U) {
        (void)snprintf(s_last_action, sizeof(s_last_action), "操作过快，已忽略");
        P4SensorPageRefresh();
        P4SensorLogTouchLatency(id ? id : "debounce", startMs, (uint32_t)esp_log_timestamp());
        return 1U;
    }
    s_last_control_ms = startMs;
    return 0U;
}

static uint32_t P4SensorAcceptFastEvent(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t nowMs;

    if (code == LV_EVENT_CLICKED) {
        return 0U;
    }
    if (code != LV_EVENT_PRESSED) {
        return 0U;
    }

    nowMs = (uint32_t)esp_log_timestamp();
    if (s_last_press_action_ms != 0U &&
        (uint32_t)(nowMs - s_last_press_action_ms) < 120U) {
        return 0U;
    }
    s_last_press_action_ms = nowMs;
    return 1U;
}

static void P4SensorBackEventCb(lv_event_t *e)
{
    if (!P4SensorAcceptFastEvent(e)) {
        return;
    }
    P4SensorPageHide();
    if (s_back_handler != NULL) {
        s_back_handler();
    }
}

static void P4SensorTabEventCb(lv_event_t *e)
{
    uintptr_t tab;

    if (!P4SensorAcceptFastEvent(e)) {
        return;
    }
    tab = (uintptr_t)lv_event_get_user_data(e);
    s_tab = (tab == (uintptr_t)P4_SENSOR_TAB_CONTROL) ? P4_SENSOR_TAB_CONTROL : P4_SENSOR_TAB_ENV;
    P4SensorBuildPage();
}

static void P4SensorQueryEventCb(lv_event_t *e)
{
    uint32_t startMs;

    if (!P4SensorAcceptFastEvent(e)) {
        return;
    }
    startMs = (uint32_t)esp_log_timestamp();
    (void)OhosUartLinkUiSensorQuery();
    (void)snprintf(s_last_action, sizeof(s_last_action), "刷新已发送");
    P4SensorPageRefresh();
    P4SensorLogTouchLatency("query", startMs, (uint32_t)esp_log_timestamp());
}

static void P4SensorFanEventCb(lv_event_t *e)
{
    uint32_t on;
    uint32_t ok;
    uint32_t startMs;

    if (!P4SensorAcceptFastEvent(e)) {
        return;
    }
    startMs = (uint32_t)esp_log_timestamp();
    if (P4SensorControlDebounced("fan", startMs)) {
        return;
    }
    on = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    ok = OhosUartLinkUiSetFan(on);
    (void)snprintf(s_last_action, sizeof(s_last_action), "风扇%s %s", on ? "开" : "关", ok ? "已发送" : "发送失败");
    P4SensorPageRefresh();
    P4SensorLogTouchLatency(on ? "fan_on" : "fan_off", startMs, (uint32_t)esp_log_timestamp());
}

static void P4SensorAlarmEventCb(lv_event_t *e)
{
    uint32_t mode;
    uint32_t ok;
    uint32_t startMs;

    if (!P4SensorAcceptFastEvent(e)) {
        return;
    }
    startMs = (uint32_t)esp_log_timestamp();
    if (P4SensorControlDebounced("alarm", startMs)) {
        return;
    }
    mode = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    ok = OhosUartLinkUiSetAlarmMode(mode);
    (void)snprintf(s_last_action,
                   sizeof(s_last_action),
                   "报警%s %s",
                   P4SensorAlarmModeText(1U, mode),
                   ok ? "已发送" : "发送失败");
    P4SensorPageRefresh();
    P4SensorLogTouchLatency("alarm", startMs, (uint32_t)esp_log_timestamp());
}

static void P4SensorLightEventCb(lv_event_t *e)
{
    uint32_t mode;
    uint32_t ok;
    uint32_t startMs;
    const char *id = "light";

    if (!P4SensorAcceptFastEvent(e)) {
        return;
    }
    startMs = (uint32_t)esp_log_timestamp();
    if (P4SensorControlDebounced("light", startMs)) {
        return;
    }
    mode = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    switch (mode) {
        case 1U:
            id = "light_red";
            ok = OhosUartLinkUiSetLight(1U, 255U, 0U, 0U, 128U);
            (void)snprintf(s_last_action, sizeof(s_last_action), "灯光红 %s", ok ? "已发送" : "发送失败");
            break;
        case 2U:
            id = "light_green";
            ok = OhosUartLinkUiSetLight(1U, 0U, 255U, 0U, 128U);
            (void)snprintf(s_last_action, sizeof(s_last_action), "灯光绿 %s", ok ? "已发送" : "发送失败");
            break;
        case 3U:
            id = "light_blue";
            ok = OhosUartLinkUiSetLight(1U, 0U, 0U, 255U, 128U);
            (void)snprintf(s_last_action, sizeof(s_last_action), "灯光蓝 %s", ok ? "已发送" : "发送失败");
            break;
        case 4U:
            id = "light_white";
            ok = OhosUartLinkUiSetLight(1U, 255U, 255U, 255U, 128U);
            (void)snprintf(s_last_action, sizeof(s_last_action), "灯光白 %s", ok ? "已发送" : "发送失败");
            break;
        case 0U:
        default:
            id = "light_off";
            ok = OhosUartLinkUiSetLight(0U, 0U, 0U, 0U, 0U);
            (void)snprintf(s_last_action, sizeof(s_last_action), "灯光关 %s", ok ? "已发送" : "发送失败");
            break;
    }
    P4SensorPageRefresh();
    P4SensorLogTouchLatency(id, startMs, (uint32_t)esp_log_timestamp());
}

static void P4SensorSceneEventCb(lv_event_t *e)
{
    uint32_t mode;
    uint32_t ok;
    uint32_t startMs;

    if (!P4SensorAcceptFastEvent(e)) {
        return;
    }
    startMs = (uint32_t)esp_log_timestamp();
    if (P4SensorControlDebounced("scene", startMs)) {
        return;
    }
    mode = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    ok = OhosUartLinkUiSetSceneMode(mode);
    (void)snprintf(s_last_action,
                   sizeof(s_last_action),
                   "%s %s",
                   P4SensorSceneModeText(1U, mode),
                   ok ? "已发送" : "发送失败");
    P4SensorPageRefresh();
    P4SensorLogTouchLatency(mode == 2U ? "scene_away" : "scene_home",
                            startMs,
                            (uint32_t)esp_log_timestamp());
}

static void P4SensorCreateBase(lv_obj_t *scr)
{
    s_ui.status = P4StatusBarCreate(scr);
    s_ui.title = P4SensorCreateLabel(scr,
                                     "设备",
                                     24,
                                     58,
                                     128,
                                     34,
                                     lv_color_make(0x17, 0x24, 0x33),
                                     LV_TEXT_ALIGN_LEFT);
    P4SensorCreateLabel(scr,
                        s_last_action[0] ? s_last_action : "智能家居控制",
                        176,
                        60,
                        280,
                        30,
                        lv_color_make(0x47, 0x55, 0x69),
                        LV_TEXT_ALIGN_RIGHT);

    s_ui.tab_env = P4SensorCreateButton(scr,
                                        "环境",
                                        24,
                                        104,
                                        204,
                                        48,
                                        s_tab == P4_SENSOR_TAB_ENV ? lv_color_make(0x10, 0xB9, 0x81) : lv_color_make(0x64, 0x74, 0x8B),
                                        P4SensorTabEventCb,
                                        (void *)(uintptr_t)P4_SENSOR_TAB_ENV);
    s_ui.tab_control = P4SensorCreateButton(scr,
                                            "控制",
                                            252,
                                            104,
                                            204,
                                            48,
                                            s_tab == P4_SENSOR_TAB_CONTROL ? lv_color_make(0x0E, 0xA5, 0xE9) : lv_color_make(0x64, 0x74, 0x8B),
                                            P4SensorTabEventCb,
                                            (void *)(uintptr_t)P4_SENSOR_TAB_CONTROL);
}

static void P4SensorCreateEnv(lv_obj_t *scr)
{
    lv_obj_t *card;

    card = P4SensorCreateRect(scr, 24, 170, 432, 152, lv_color_white(), 8);
    P4SensorCreateLabel(card, "温湿度", 18, 14, 120, 26, lv_color_make(0x47, 0x55, 0x69), LV_TEXT_ALIGN_LEFT);
    s_ui.summary = P4SensorCreateLabel(card, "--.-- C  --.-- %", 18, 52, 260, 42,
                                       lv_color_make(0x10, 0xB9, 0x81), LV_TEXT_ALIGN_LEFT);
    s_ui.line1 = P4SensorCreateLabel(card, "在线 --  Report 0", 18, 104, 380, 28,
                                     lv_color_make(0x64, 0x74, 0x8B), LV_TEXT_ALIGN_LEFT);

    card = P4SensorCreateRect(scr, 24, 338, 432, 180, lv_color_white(), 8);
    P4SensorCreateLabel(card, "四模组状态", 18, 14, 160, 26,
                        lv_color_make(0x47, 0x55, 0x69), LV_TEXT_ALIGN_LEFT);
    s_ui.line2 = P4SensorCreateLabel(card, "风扇 --", 18, 50, 390, 28,
                                     lv_color_make(0x02, 0x84, 0xC7), LV_TEXT_ALIGN_LEFT);
    s_ui.line3 = P4SensorCreateLabel(card, "报警器 --", 18, 88, 390, 28,
                                     lv_color_make(0xE1, 0x1D, 0x48), LV_TEXT_ALIGN_LEFT);
    s_ui.line4 = P4SensorCreateLabel(card, "照明 --", 18, 126, 390, 28,
                                     lv_color_make(0x16, 0xA3, 0x4A), LV_TEXT_ALIGN_LEFT);

    card = P4SensorCreateRect(scr, 24, 534, 432, 102, lv_color_white(), 8);
    P4SensorCreateLabel(card, "场景状态", 18, 14, 120, 26,
                        lv_color_make(0x47, 0x55, 0x69), LV_TEXT_ALIGN_LEFT);
    s_ui.ack1 = P4SensorCreateLabel(card, "当前 --", 18, 54, 190, 28,
                                    lv_color_make(0x7C, 0x3A, 0xED), LV_TEXT_ALIGN_LEFT);
    s_ui.ack2 = P4SensorCreateLabel(card, "等待 Hub 上报", 210, 54, 196, 28,
                                    lv_color_make(0x64, 0x74, 0x8B), LV_TEXT_ALIGN_RIGHT);
}

static void P4SensorCreateControl(lv_obj_t *scr)
{
    lv_obj_t *card;

    card = P4SensorCreateRect(scr, 24, 170, 432, 108, lv_color_white(), 8);
    P4SensorCreateLabel(card, "家居模式", 18, 12, 150, 24, lv_color_make(0x47, 0x55, 0x69), LV_TEXT_ALIGN_LEFT);
    s_ui.scene = P4SensorCreateLabel(card, "当前 --", 18, 42, 180, 26,
                                     lv_color_make(0x7C, 0x3A, 0xED), LV_TEXT_ALIGN_LEFT);
    s_ui.scene_ack = P4SensorCreateLabel(card, "未控制", 206, 44, 200, 24,
                                         lv_color_make(0x64, 0x74, 0x8B), LV_TEXT_ALIGN_RIGHT);
    P4SensorCreateButton(card, "居家", 18, 72, 186, 28, lv_color_make(0x10, 0xB9, 0x81),
                         P4SensorSceneEventCb, (void *)(uintptr_t)1U);
    P4SensorCreateButton(card, "离家", 220, 72, 186, 28, lv_color_make(0xF5, 0x9E, 0x0B),
                         P4SensorSceneEventCb, (void *)(uintptr_t)2U);

    card = P4SensorCreateRect(scr, 24, 290, 432, 104, lv_color_white(), 8);
    P4SensorCreateLabel(card, "智能风扇", 18, 14, 150, 26, lv_color_make(0x47, 0x55, 0x69), LV_TEXT_ALIGN_LEFT);
    s_ui.line1 = P4SensorCreateLabel(card, "风扇 --", 18, 42, 180, 24,
                                     lv_color_make(0x02, 0x84, 0xC7), LV_TEXT_ALIGN_LEFT);
    s_ui.ack1 = P4SensorCreateLabel(card, "未控制", 206, 44, 200, 24,
                                    lv_color_make(0x64, 0x74, 0x8B), LV_TEXT_ALIGN_RIGHT);
    P4SensorCreateButton(card, "开", 18, 72, 186, 28, lv_color_make(0x0E, 0xA5, 0xE9),
                         P4SensorFanEventCb, (void *)(uintptr_t)1U);
    P4SensorCreateButton(card, "关", 220, 72, 186, 28, lv_color_make(0x64, 0x74, 0x8B),
                         P4SensorFanEventCb, (void *)(uintptr_t)0U);

    card = P4SensorCreateRect(scr, 24, 406, 432, 114, lv_color_white(), 8);
    P4SensorCreateLabel(card, "声光报警", 18, 14, 150, 26, lv_color_make(0x47, 0x55, 0x69), LV_TEXT_ALIGN_LEFT);
    s_ui.line2 = P4SensorCreateLabel(card, "报警 --", 18, 42, 180, 24,
                                     lv_color_make(0xE1, 0x1D, 0x48), LV_TEXT_ALIGN_LEFT);
    s_ui.ack2 = P4SensorCreateLabel(card, "未控制", 206, 44, 200, 24,
                                    lv_color_make(0x64, 0x74, 0x8B), LV_TEXT_ALIGN_RIGHT);
    P4SensorCreateButton(card, "关", 18, 76, 60, 28, lv_color_make(0x64, 0x74, 0x8B),
                         P4SensorAlarmEventCb, (void *)(uintptr_t)0U);
    P4SensorCreateButton(card, "常亮", 86, 76, 72, 28, lv_color_make(0xF4, 0x3F, 0x5E),
                         P4SensorAlarmEventCb, (void *)(uintptr_t)1U);
    P4SensorCreateButton(card, "闪烁", 166, 76, 72, 28, lv_color_make(0xF4, 0x3F, 0x5E),
                         P4SensorAlarmEventCb, (void *)(uintptr_t)2U);
    P4SensorCreateButton(card, "蜂鸣", 246, 76, 72, 28, lv_color_make(0xF4, 0x3F, 0x5E),
                         P4SensorAlarmEventCb, (void *)(uintptr_t)3U);
    P4SensorCreateButton(card, "组合", 326, 76, 80, 28, lv_color_make(0xF4, 0x3F, 0x5E),
                         P4SensorAlarmEventCb, (void *)(uintptr_t)4U);

    card = P4SensorCreateRect(scr, 24, 532, 432, 104, lv_color_white(), 8);
    P4SensorCreateLabel(card, "智能灯光", 18, 14, 150, 26, lv_color_make(0x47, 0x55, 0x69), LV_TEXT_ALIGN_LEFT);
    s_ui.line3 = P4SensorCreateLabel(card, "实际 -- RGB --", 18, 42, 188, 24,
                                     lv_color_make(0x16, 0xA3, 0x4A), LV_TEXT_ALIGN_LEFT);
    s_ui.ack3 = P4SensorCreateLabel(card, "命令 未控制", 206, 44, 200, 24,
                                    lv_color_make(0x64, 0x74, 0x8B), LV_TEXT_ALIGN_RIGHT);
    P4SensorCreateButton(card, "关", 18, 74, 70, 26, lv_color_make(0x64, 0x74, 0x8B),
                         P4SensorLightEventCb, (void *)(uintptr_t)0U);
    P4SensorCreateButton(card, "红", 98, 74, 70, 26, lv_color_make(0xEF, 0x44, 0x44),
                         P4SensorLightEventCb, (void *)(uintptr_t)1U);
    P4SensorCreateButton(card, "绿", 178, 74, 70, 26, lv_color_make(0x22, 0xC5, 0x5E),
                         P4SensorLightEventCb, (void *)(uintptr_t)2U);
    P4SensorCreateButton(card, "蓝", 258, 74, 70, 26, lv_color_make(0x3B, 0x82, 0xF6),
                         P4SensorLightEventCb, (void *)(uintptr_t)3U);
    P4SensorCreateButton(card, "白", 338, 74, 68, 26, lv_color_make(0x0F, 0x76, 0x6E),
                         P4SensorLightEventCb, (void *)(uintptr_t)4U);
}

static void P4SensorBuildPage(void)
{
    lv_obj_t *scr = lv_screen_active();

    if (scr == NULL) {
        return;
    }

    (void)memset(&s_ui, 0, sizeof(s_ui));
    lv_obj_clean(scr);
    P4SystemFontApply(scr);
    lv_obj_set_style_bg_color(scr, lv_color_make(0xF1, 0xF5, 0xF9), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    P4SensorCreateBase(scr);
    if (s_tab == P4_SENSOR_TAB_CONTROL) {
        P4SensorCreateControl(scr);
    } else {
        P4SensorCreateEnv(scr);
    }

    P4SensorCreateButton(scr, "刷新", 24, 704, 204, 58, lv_color_make(0x0F, 0x76, 0x6E),
                         P4SensorQueryEventCb, NULL);
    s_ui.back = P4SensorCreateButton(scr, "返回主控台", 252, 704, 204, 58, lv_color_make(0x33, 0x41, 0x55),
                                     P4SensorBackEventCb, NULL);

    s_visible = 1U;
    s_have_fingerprint = 0U;
    P4SensorPageRefresh();
}

void P4SensorPageSetBackHandler(P4SensorPageBackHandler handler)
{
    s_back_handler = handler;
}

void P4SensorPageHide(void)
{
    s_visible = 0U;
    s_have_fingerprint = 0U;
    (void)memset(&s_ui, 0, sizeof(s_ui));
}

void P4SensorPageRefresh(void)
{
    P4UiModelSnapshot model;
    const OhosUartLinkSensorSnapshot *s;
    char text[128];
    char value1[32];
    char value2[32];
    char cmd[56];

    if (!s_visible) {
        return;
    }

    P4UiModelGetSnapshot(&model);
    s = &model.sensor_th;
    s_last_fingerprint = P4SensorSnapshotFingerprint(s);
    s_have_fingerprint = 1U;
    s_last_periodic_refresh_ms = (uint32_t)esp_log_timestamp();

    if (s_ui.status != NULL && lv_obj_is_valid(s_ui.status)) {
        P4StatusBarUpdate(s_ui.status, &model);
    }

    if (s_tab == P4_SENSOR_TAB_ENV) {
        if (s->valid) {
            P4SensorFormatFixed100(value1, sizeof(value1), s->temp100, "C");
            P4SensorFormatFixed100(value2, sizeof(value2), s->humi100, "%");
            (void)snprintf(text, sizeof(text), "%s  %s", value1, value2);
        } else {
            (void)snprintf(text, sizeof(text), "--.-- C  --.-- %%");
        }
        P4SensorSetLabel(s_ui.summary, text);
        (void)snprintf(text,
                       sizeof(text),
                       "在线 %s  Report %u  Query %u/%u",
                       P4SensorOnlineText(s->sht30_online_valid, s->sht30_online),
                       (unsigned)s->rx_count,
                       (unsigned)s->query_tx_ok,
                       (unsigned)s->query_tx_count);
        P4SensorSetLabel(s_ui.line1, text);

        (void)snprintf(text,
                       sizeof(text),
                       "风扇 %s  在线 %s",
                       P4SensorOnOffText(s->fan_switch_valid, s->fan_switch_on),
                       P4SensorOnlineText(s->fan_online_valid, s->fan_online));
        P4SensorSetLabel(s_ui.line2, text);

        (void)snprintf(text,
                       sizeof(text),
                       "报警器 %s  在线 %s",
                       P4SensorAlarmModeText(s->alarm_mode_valid, s->alarm_mode),
                       P4SensorOnlineText(s->alarm_online_valid, s->alarm_online));
        P4SensorSetLabel(s_ui.line3, text);

        (void)snprintf(text,
                       sizeof(text),
                       "照明 %s  RGB %s%u,%u,%u  在线 %s",
                       P4SensorOnOffText(s->light_switch_valid, s->light_switch_on),
                       s->light_rgb_valid ? "" : "--",
                       (unsigned)(s->light_rgb_valid ? s->light_r : 0U),
                       (unsigned)(s->light_rgb_valid ? s->light_g : 0U),
                       (unsigned)(s->light_rgb_valid ? s->light_b : 0U),
                       P4SensorOnlineText(s->light_online_valid, s->light_online));
        P4SensorSetLabel(s_ui.line4, text);

        P4SensorFormatSceneCause(cmd, sizeof(cmd), s->scene_cause_valid, s->scene_cause);
        (void)snprintf(text,
                       sizeof(text),
                       "当前 %s",
                       P4SensorSceneModeText(s->scene_mode_valid, s->scene_mode));
        P4SensorSetLabel(s_ui.ack1, text);
        (void)snprintf(text,
                       sizeof(text),
                       "%s  R%u",
                       cmd,
                       (unsigned)s->scene_report_count);
        P4SensorSetLabel(s_ui.ack2, text);
        return;
    }

    P4SensorFormatSceneCause(cmd, sizeof(cmd), s->scene_cause_valid, s->scene_cause);
    (void)snprintf(text,
                   sizeof(text),
                   "当前 %s  %s",
                   P4SensorSceneModeText(s->scene_mode_valid, s->scene_mode),
                   cmd);
    P4SensorSetLabel(s_ui.scene, text);
    P4SensorFormatSceneCommand(cmd, sizeof(cmd), s);
    (void)snprintf(text,
                   sizeof(text),
                   "%s R%u",
                   cmd,
                   (unsigned)s->scene_report_count);
    P4SensorSetLabel(s_ui.scene_ack, text);

    (void)snprintf(text,
                   sizeof(text),
                   "风扇 %s  Level %s%u  在线 %s",
                   P4SensorOnOffText(s->fan_switch_valid, s->fan_switch_on),
                   s->fan_level_valid ? "" : "--",
                   (unsigned)(s->fan_level_valid ? s->fan_level : 0U),
                   P4SensorOnlineText(s->fan_online_valid, s->fan_online));
    P4SensorSetLabel(s_ui.line1, text);
    P4SensorSetLabel(s_ui.ack1,
                     P4SensorAckText(s->fan_control_tx_count,
                                     s->fan_control_tx_ok,
                                     s->fan_control_ack_count,
                                     s->fan_control_ack_error));

    (void)snprintf(text,
                   sizeof(text),
                   "报警 %s  在线 %s",
                   P4SensorAlarmModeText(s->alarm_mode_valid, s->alarm_mode),
                   P4SensorOnlineText(s->alarm_online_valid, s->alarm_online));
    P4SensorSetLabel(s_ui.line2, text);
    P4SensorSetLabel(s_ui.ack2,
                     P4SensorAckText(s->alarm_control_tx_count,
                                     s->alarm_control_tx_ok,
                                     s->alarm_control_ack_count,
                                     s->alarm_control_ack_error));

    (void)snprintf(text,
                   sizeof(text),
                   "实际 %s RGB %s%u,%u,%u B%u G%u",
                   P4SensorOnOffText(s->light_switch_valid, s->light_switch_on),
                   s->light_rgb_valid ? "" : "--",
                   (unsigned)(s->light_rgb_valid ? s->light_r : 0U),
                   (unsigned)(s->light_rgb_valid ? s->light_g : 0U),
                   (unsigned)(s->light_rgb_valid ? s->light_b : 0U),
                   (unsigned)(s->light_rgb_valid ? s->light_brightness : 0U),
                   (unsigned)(s->light_gpio_output_valid ? s->light_gpio_output : 0U));
    P4SensorSetLabel(s_ui.line3, text);
    P4SensorFormatLightCommand(cmd, sizeof(cmd), s);
    (void)snprintf(text, sizeof(text), "命令 %s", cmd);
    P4SensorSetLabel(s_ui.ack3, text);
}

void P4SensorPageShow(void)
{
    s_tab = P4_SENSOR_TAB_ENV;
    s_last_action[0] = '\0';
    s_last_control_ms = 0U;
    s_last_press_action_ms = 0U;
    s_last_periodic_refresh_ms = 0U;
    (void)OhosUartLinkUiSensorQuery();
    P4SensorBuildPage();
    ESP_LOGI(TAG, "sensor fixed device page created");
}

void P4SensorPageProcessPending(void)
{
    OhosUartLinkSensorSnapshot sensor;
    uint32_t fingerprint;
    uint32_t nowMs;

    if (!s_visible) {
        return;
    }

    nowMs = (uint32_t)esp_log_timestamp();
    (void)memset(&sensor, 0, sizeof(sensor));
    OhosUartLinkUiGetSensorSnapshot(&sensor);
    fingerprint = P4SensorSnapshotFingerprint(&sensor);
    if (!s_have_fingerprint ||
        fingerprint != s_last_fingerprint ||
        s_last_periodic_refresh_ms == 0U ||
        (uint32_t)(nowMs - s_last_periodic_refresh_ms) >= P4_SENSOR_PERIODIC_REFRESH_MS) {
        P4SensorPageRefresh();
    }
}

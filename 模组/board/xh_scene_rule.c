#include "xh_scene_rule.h"

#include <stdio.h>
#include <string.h>

#include "xh_fan_control.h"
#include "xh_module_ids.h"
#include "xh_sensor_state.h"
#include "xh_sle_hub.h"
#include "xh_sle_proto.h"

/* ---- Light modes (档位) ----
 * 0 = off  关
 * 1 = warm 暖光  (一档)
 * 2 = cold 冷光  (二档)
 * 3 = dim  暗光  (三档)
 */
#define XH_LIGHT_MODE_OFF  0U
#define XH_LIGHT_MODE_WARM 1U
#define XH_LIGHT_MODE_COLD 2U
#define XH_LIGHT_MODE_DIM  3U

/* ---- Fan levels ---- */
#define XH_FAN_LEVEL_OFF  0U
#define XH_FAN_LEVEL_LOW  1U
#define XH_FAN_LEVEL_MID  2U
#define XH_FAN_LEVEL_HIGH 3U

/* ---- Scene modes ---- */
#define XH_SCENE_MODE_HOME  1U
#define XH_SCENE_MODE_AWAY  2U
#define XH_SCENE_MODE_SLEEP 3U

/* ---- HOME fan thresholds (temp in 0.01°C, humi in 0.01%) ---- */
#ifndef XH_HOME_FAN_HIGH_TRIGGER_TEMP100
#define XH_HOME_FAN_HIGH_TRIGGER_TEMP100 3500
#endif
#ifndef XH_HOME_FAN_MID_TRIGGER_TEMP100
#define XH_HOME_FAN_MID_TRIGGER_TEMP100 3200
#endif
#ifndef XH_HOME_FAN_LOW_TRIGGER_TEMP100
#define XH_HOME_FAN_LOW_TRIGGER_TEMP100 2800
#endif
#ifndef XH_HOME_FAN_RELEASE_TEMP100
#define XH_HOME_FAN_RELEASE_TEMP100 2600
#endif
#ifndef XH_HOME_FAN_MID_TRIGGER_HUMI100
#define XH_HOME_FAN_MID_TRIGGER_HUMI100 6000
#endif
#ifndef XH_HOME_FAN_LOW_TRIGGER_HUMI100
#define XH_HOME_FAN_LOW_TRIGGER_HUMI100 5000
#endif
#ifndef XH_HOME_FAN_RELEASE_HUMI100
#define XH_HOME_FAN_RELEASE_HUMI100 4000
#endif

/* ---- SLEEP fan thresholds (temperature only) ---- */
#ifndef XH_SLEEP_FAN_MID_TRIGGER_TEMP100
#define XH_SLEEP_FAN_MID_TRIGGER_TEMP100 3300
#endif
#ifndef XH_SLEEP_FAN_LOW_TRIGGER_TEMP100
#define XH_SLEEP_FAN_LOW_TRIGGER_TEMP100 3000
#endif
#ifndef XH_SLEEP_FAN_RELEASE_TEMP100
#define XH_SLEEP_FAN_RELEASE_TEMP100 2800
#endif

#define XH_SCENE_CAUSE_TEMP_HIGH 0x01U
#define XH_SCENE_CAUSE_HUMI_HIGH 0x02U
#define XH_SCENE_CAUSE_ACT_FAIL  0x10U

static uint8_t g_scene_mode = XH_SCENE_MODE_HOME;
static uint8_t g_scene_cause;
static bool g_fan_auto_inhibit;
static bool g_scene_auto_mode;
static uint8_t g_current_fan_level;
static uint8_t g_current_light_mode;

static void xh_scene_report(void)
{
    xh_sle_hub_send_scene_report(true);
}

static bool xh_scene_send_fan(uint8_t level)
{
    if (xh_fan_control_set_level(level)) {
        return true;
    }
    g_scene_cause |= XH_SCENE_CAUSE_ACT_FAIL;
    printf("[xh_scene] fan level=%u failed\r\n", level);
    return false;
}

static bool xh_scene_send_light(uint8_t mode)
{
    if (xh_sle_hub_send_light_control(mode)) {
        return true;
    }
    g_scene_cause |= XH_SCENE_CAUSE_ACT_FAIL;
    printf("[xh_scene] light mode=%u failed\r\n", mode);
    return false;
}

static bool xh_scene_home_fan_release(const xh_sht30_state_t *th)
{
    /* Fan turns off when temp drops below the release threshold.
     * Humidity is intentionally ignored. */
    return th->temp100 <= XH_HOME_FAN_RELEASE_TEMP100;
}

static bool xh_scene_sleep_fan_release(const xh_sht30_state_t *th)
{
    return th->temp100 <= XH_SLEEP_FAN_RELEASE_TEMP100;
}

static void xh_scene_apply_home_rule(const xh_sht30_state_t *th)
{
    if (xh_scene_home_fan_release(th)) {
        if (g_fan_auto_inhibit) {
            printf("[xh_scene] home fan manual inhibit cleared temp=%ld humi=%ld\r\n",
                (long)th->temp100, (long)th->humi100);
        }
        g_fan_auto_inhibit = false;
        if (g_current_fan_level != XH_FAN_LEVEL_OFF) {
            printf("[xh_scene] home release fan off temp=%ld humi=%ld\r\n",
                (long)th->temp100, (long)th->humi100);
            if (xh_scene_send_fan(XH_FAN_LEVEL_OFF)) {
                g_current_fan_level = XH_FAN_LEVEL_OFF;
            }
        }
        return;
    }

    if (g_fan_auto_inhibit) {
        return;
    }

    /* 温度达标且风扇当前关闭 -> 默认开一档；
     * 风扇已开启时不自动升降档，保留用户通过 key 手动设定的档位。
     * 风扇开关只看温度，与湿度无关。 */
    if (g_current_fan_level != XH_FAN_LEVEL_OFF) {
        return;
    }

    bool conditions_met = th->temp100 >= XH_HOME_FAN_LOW_TRIGGER_TEMP100;
    if (conditions_met) {
        printf("[xh_scene] home fan auto on level=1 temp=%ld humi=%ld\r\n",
            (long)th->temp100, (long)th->humi100);
        if (xh_scene_send_fan(XH_FAN_LEVEL_LOW)) {
            g_current_fan_level = XH_FAN_LEVEL_LOW;
        }
    }
}

static void xh_scene_apply_sleep_rule(const xh_sht30_state_t *th)
{
    if (xh_scene_sleep_fan_release(th)) {
        if (g_current_fan_level != XH_FAN_LEVEL_OFF) {
            printf("[xh_scene] sleep release fan off temp=%ld\r\n",
                (long)th->temp100);
            if (xh_scene_send_fan(XH_FAN_LEVEL_OFF)) {
                g_current_fan_level = XH_FAN_LEVEL_OFF;
            }
        }
        return;
    }

    /* 温度达标且风扇当前关闭 -> 默认开一档；已开启时保留用户手动档位。 */
    if (g_current_fan_level != XH_FAN_LEVEL_OFF) {
        return;
    }

    if (th->temp100 >= XH_SLEEP_FAN_LOW_TRIGGER_TEMP100) {
        printf("[xh_scene] sleep fan auto on level=1 temp=%ld\r\n",
            (long)th->temp100);
        if (xh_scene_send_fan(XH_FAN_LEVEL_LOW)) {
            g_current_fan_level = XH_FAN_LEVEL_LOW;
        }
    }
}

static void xh_scene_apply_light_for_mode(uint8_t mode)
{
    uint8_t want_mode;
    if (mode == XH_SCENE_MODE_HOME) {
        /* 居家（人在时）：自动开一档暖光 */
        want_mode = XH_LIGHT_MODE_WARM;
    } else if (mode == XH_SCENE_MODE_SLEEP) {
        /* 睡眠模式：自动开三档暗光 */
        want_mode = XH_LIGHT_MODE_DIM;
    } else {
        /* 离家模式：自动灭灯 */
        want_mode = XH_LIGHT_MODE_OFF;
    }

    if (want_mode == g_current_light_mode) {
        return;
    }

    printf("[xh_scene] light mode %u->%u\r\n", g_current_light_mode, want_mode);
    if (xh_scene_send_light(want_mode)) {
        g_current_light_mode = want_mode;
    }
}

static void xh_scene_auto_switch_from_presence(void)
{
    xh_presence_state_t presence;
    if (!xh_sensor_state_get_presence(&presence)) {
        return;
    }

    if (presence.present && g_scene_mode == XH_SCENE_MODE_AWAY) {
        printf("[xh_scene] presence detected -> auto HOME\r\n");
        g_scene_auto_mode = true;
        (void)xh_scene_rule_set_mode(XH_SCENE_MODE_HOME, "ld2401");
    } else if (!presence.present && g_scene_mode == XH_SCENE_MODE_HOME) {
        printf("[xh_scene] presence cleared -> auto AWAY\r\n");
        g_scene_auto_mode = true;
        (void)xh_scene_rule_set_mode(XH_SCENE_MODE_AWAY, "ld2401");
    } else if (presence.present && g_scene_mode == XH_SCENE_MODE_HOME) {
        /* Already HOME and still present: ensure the light reflects HOME
         * mode. On power-up the scene starts in HOME with light OFF; if
         * someone is already present we must push the warm-light command
         * so the light turns on without waiting for a mode transition. */
        if (g_current_light_mode != XH_LIGHT_MODE_WARM) {
            printf("[xh_scene] presence still present, force HOME light\r\n");
            xh_scene_apply_light_for_mode(XH_SCENE_MODE_HOME);
        }
    }
}

void xh_scene_rule_init(void)
{
    g_scene_mode = XH_SCENE_MODE_HOME;
    g_scene_cause = 0U;
    g_fan_auto_inhibit = false;
    g_scene_auto_mode = true;
    g_current_fan_level = XH_FAN_LEVEL_OFF;
    g_current_light_mode = XH_LIGHT_MODE_OFF;

    printf("[xh_scene] init HOME "
        "home_fan T<%d/%d/%d R=%d H<%d/%d R=%d "
        "sleep_fan T<%d/%d R=%d\r\n",
        XH_HOME_FAN_LOW_TRIGGER_TEMP100, XH_HOME_FAN_MID_TRIGGER_TEMP100,
        XH_HOME_FAN_HIGH_TRIGGER_TEMP100, XH_HOME_FAN_RELEASE_TEMP100,
        XH_HOME_FAN_LOW_TRIGGER_HUMI100, XH_HOME_FAN_MID_TRIGGER_HUMI100,
        XH_HOME_FAN_RELEASE_HUMI100,
        XH_SLEEP_FAN_LOW_TRIGGER_TEMP100, XH_SLEEP_FAN_MID_TRIGGER_TEMP100,
        XH_SLEEP_FAN_RELEASE_TEMP100);
}

bool xh_scene_rule_set_mode(uint8_t mode, const char *source)
{
    const char *src = (source != NULL) ? source : "unknown";

    if (mode != XH_SCENE_MODE_HOME &&
        mode != XH_SCENE_MODE_AWAY &&
        mode != XH_SCENE_MODE_SLEEP) {
        printf("[xh_scene] invalid mode=%u source=%s\r\n",
            (unsigned int)mode, src);
        return false;
    }

    if (mode == g_scene_mode) {
        return true;
    }

    printf("[xh_scene] set mode %u->%u source=%s\r\n",
        (unsigned int)g_scene_mode, (unsigned int)mode, src);

    /* Track whether mode was set by LD2401 auto or manual command */
    if (source != NULL && strcmp(source, "ld2401") == 0) {
        g_scene_auto_mode = true;
    } else {
        g_scene_auto_mode = false;
    }

    g_scene_mode = mode;
    g_scene_cause = 0U;
    g_fan_auto_inhibit = false;

    /* Apply light for the new mode */
    xh_scene_apply_light_for_mode(mode);

    /* Turn off fan when leaving a mode (will be re-evaluated by sensor update) */
    if (g_current_fan_level != XH_FAN_LEVEL_OFF) {
        (void)xh_scene_send_fan(XH_FAN_LEVEL_OFF);
        g_current_fan_level = XH_FAN_LEVEL_OFF;
    }

    /* AWAY mode: fan stays off regardless */
    if (mode == XH_SCENE_MODE_AWAY) {
        /* nothing more to do */
    }

    /* Re-evaluate fan based on current sensor data */
    xh_scene_rule_on_sensor_update(XH_MODULE_ID_SHT30);
    xh_scene_report();
    return true;
}

uint8_t xh_scene_rule_get_mode(void)
{
    return g_scene_mode;
}

uint8_t xh_scene_rule_get_cause(void)
{
    return g_scene_cause;
}

uint16_t xh_scene_rule_pack_report(uint8_t *out, uint16_t cap)
{
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t flags = 0U;

    if (g_current_light_mode != XH_LIGHT_MODE_OFF) {
        flags |= 0x01U;
    }
    if (g_current_fan_level != XH_FAN_LEVEL_OFF) {
        flags |= 0x02U;
    }

    xh_presence_state_t presence;
    if (xh_sensor_state_get_presence(&presence) && presence.present) {
        flags |= 0x04U;
    }
    if (g_scene_auto_mode) {
        flags |= 0x08U;
    }

    if (out == NULL || cap < XH_PROTO_HDR_LEN) {
        return 0U;
    }

    if (!xh_proto_begin(out, cap, xh_proto_next_seq(),
            XH_MODULE_ID_HUB_SCENE, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1U) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SCENE_MODE, g_scene_mode) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SCENE_CAUSE, g_scene_cause) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SCENE_FLAGS, flags) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_FAN_LEVEL, g_current_fan_level) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_LIGHT_MODE, g_current_light_mode) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0U;
    }
    return len;
}

void xh_scene_rule_on_manual_fan_control(uint8_t level, const char *source)
{
    xh_sht30_state_t th;
    const char *src = (source != NULL) ? source : "unknown";

    g_current_fan_level = level;

    if (g_scene_mode != XH_SCENE_MODE_HOME || level != 0) {
        if (level != 0) {
            g_fan_auto_inhibit = false;
        }
        return;
    }

    if (!xh_sensor_state_get_sht30(&th)) {
        return;
    }

    bool conditions_met = th.temp100 >= XH_HOME_FAN_LOW_TRIGGER_TEMP100;
    if (conditions_met) {
        g_fan_auto_inhibit = true;
        printf("[xh_scene] home manual fan off inhibits auto source=%s temp=%ld humi=%ld\r\n",
            src, (long)th.temp100, (long)th.humi100);
    }
}

void xh_scene_rule_on_fan_report_transition(uint8_t old_level, uint8_t new_level,
                                            const char *source)
{
    if (g_scene_mode == XH_SCENE_MODE_HOME && old_level != 0 && new_level == 0) {
        xh_scene_rule_on_manual_fan_control(0, source);
    } else {
        g_current_fan_level = new_level;
    }
}

void xh_scene_rule_on_light_report(uint8_t mode, const char *source)
{
    const char *src = (source != NULL) ? source : "unknown";
    if (mode == g_current_light_mode) {
        return;
    }
    printf("[xh_scene] light report mode %u->%u source=%s\r\n",
        (unsigned int)g_current_light_mode, (unsigned int)mode, src);
    g_current_light_mode = mode;
}

void xh_scene_rule_on_sensor_update(uint8_t module_id)
{
    xh_sht30_state_t th;

    if (module_id == XH_MODULE_ID_LD2401) {
        if (g_scene_mode != XH_SCENE_MODE_SLEEP) {
            xh_scene_auto_switch_from_presence();
        }
        return;
    }

    if (module_id != XH_MODULE_ID_SHT30 ||
        !xh_sensor_state_get_sht30(&th)) {
        return;
    }

    if (g_scene_mode == XH_SCENE_MODE_HOME) {
        xh_scene_apply_home_rule(&th);
    } else if (g_scene_mode == XH_SCENE_MODE_SLEEP) {
        xh_scene_apply_sleep_rule(&th);
    }
    /* AWAY: fan stays off, nothing to do */
}

void xh_scene_rule_tick(void)
{
    /* Rules run when a fresh SHT30 or LD2401 report arrives. */
}

void xh_scene_rule_on_module_ready(uint8_t module_id)
{
    printf("[xh_scene] module ready=%u, force-push current scene mode=%u\r\n",
        (unsigned int)module_id, (unsigned int)g_scene_mode);

    if (module_id == XH_MODULE_ID_LIGHT) {
        /* Force re-apply the light mode for the current scene. Reset the
         * tracked mode to OFF so apply_light_for_mode will always send the
         * command (it skips when want_mode == g_current_light_mode). */
        g_current_light_mode = XH_LIGHT_MODE_OFF;
        xh_scene_apply_light_for_mode(g_scene_mode);
    } else if (module_id == XH_MODULE_ID_LD2401) {
        /* Presence sensor just connected: re-evaluate presence state so
         * that if someone is already present on power-up, the scene
         * switches (or stays) HOME and the light turns on. */
        xh_scene_auto_switch_from_presence();
    } else if (module_id == XH_MODULE_ID_SHT30) {
        /* TH sensor just connected: re-evaluate fan rules with any
         * cached TH data. */
        xh_scene_rule_on_sensor_update(XH_MODULE_ID_SHT30);
    }
}

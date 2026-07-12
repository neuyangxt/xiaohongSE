#include "p4_desktop_page.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "p4_status_bar.h"
#include "p4_system_font.h"
#include "p4_ui_model.h"

static const char *TAG = "p4_desktop";

typedef enum {
    P4_DESKTOP_ICON_CHAT = 0,
    P4_DESKTOP_ICON_SETTINGS,
    P4_DESKTOP_ICON_WIFI,
    P4_DESKTOP_ICON_INFO,
    P4_DESKTOP_ICON_DIAG,
    P4_DESKTOP_ICON_SENSOR,
    P4_DESKTOP_ICON_OTA,
    P4_DESKTOP_ICON_CAMERA
} P4DesktopIconType;

typedef struct {
    P4DesktopEntry entry;
    const char *title;
    const char *subtitle;
    P4DesktopIconType icon;
    uint32_t disabled;
} P4DesktopItemDef;

typedef struct {
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *badge;
    lv_obj_t *icon_root;
} P4DesktopItemView;

static const P4DesktopItemDef s_item_defs[P4_DESKTOP_ENTRY_COUNT] = {
    {P4_DESKTOP_ENTRY_DIALOG,       "对话",     "语音交互", P4_DESKTOP_ICON_CHAT,     0U},
    {P4_DESKTOP_ENTRY_SETTINGS,     "设置",     "音量亮度", P4_DESKTOP_ICON_SETTINGS, 0U},
    {P4_DESKTOP_ENTRY_WIFI,         "Wi-Fi",    "网络连接", P4_DESKTOP_ICON_WIFI,     0U},
    {P4_DESKTOP_ENTRY_DEVICE_INFO,  "设备信息", "版本身份", P4_DESKTOP_ICON_INFO,     0U},
    {P4_DESKTOP_ENTRY_DIAGNOSTICS,  "诊断",     "工程测试", P4_DESKTOP_ICON_DIAG,     0U},
    {P4_DESKTOP_ENTRY_SENSOR,       "传感器数据", "温湿度", P4_DESKTOP_ICON_SENSOR,   0U},
    {P4_DESKTOP_ENTRY_OTA,          "OTA",      "升级信息", P4_DESKTOP_ICON_OTA,      0U},
    {P4_DESKTOP_ENTRY_CAMERA,       "相机",     "预览测试", P4_DESKTOP_ICON_CAMERA,   0U},
};

static P4DesktopPageOps s_ops;
static P4DesktopItemView s_items[P4_DESKTOP_ENTRY_COUNT];
static lv_obj_t *s_status_label;
static lv_obj_t *s_state_label;
static lv_obj_t *s_hint_label;
static lv_obj_t *s_footer_label;
static uint32_t s_visible;
static uint32_t s_focus_index;
static char s_sensor_badge[24];

static lv_obj_t *P4DesktopCreateRect(lv_obj_t *parent,
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
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    return obj;
}

static void P4DesktopMakeIconObj(lv_obj_t *obj)
{
    if (obj == NULL || !lv_obj_is_valid(obj)) {
        return;
    }

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
}

static void P4DesktopSetText(lv_obj_t *label, const char *text)
{
    if (label == NULL || text == NULL || !lv_obj_is_valid(label)) {
        return;
    }
    lv_label_set_text(label, text);
}

static const char *P4DesktopEntryBadge(P4DesktopEntry entry, const P4UiModelSnapshot *model)
{
    if (model == NULL) {
        return "--";
    }

    switch (entry) {
        case P4_DESKTOP_ENTRY_DIALOG:
            return P4UiModelDialogStateText(model->dialog_state);
        case P4_DESKTOP_ENTRY_SETTINGS:
            return model->interrupt_mode ? "打断开" : "打断关";
        case P4_DESKTOP_ENTRY_WIFI:
            return model->uart.link_ready ? "已连接" : "等待";
        case P4_DESKTOP_ENTRY_DEVICE_INFO:
            return model->uart.handshake_done ? "已握手" : "未握手";
        case P4_DESKTOP_ENTRY_DIAGNOSTICS:
            return "可用";
        case P4_DESKTOP_ENTRY_SENSOR:
            if (model->sensor_th.valid) {
                int32_t temp100 = model->sensor_th.temp100;
                const char *sign = "";
                if (temp100 < 0) {
                    sign = "-";
                    temp100 = -temp100;
                }
                (void)snprintf(s_sensor_badge,
                               sizeof(s_sensor_badge),
                               "%s%d.%02dC",
                               sign,
                               (int)(temp100 / 100),
                               (int)(temp100 % 100));
                return s_sensor_badge;
            }
            return "等待";
        case P4_DESKTOP_ENTRY_OTA:
            return "待接入";
        case P4_DESKTOP_ENTRY_CAMERA:
            return "可用";
        default:
            return "--";
    }
}

static lv_color_t P4DesktopEntryAccent(P4DesktopEntry entry)
{
    switch (entry) {
        case P4_DESKTOP_ENTRY_DIALOG:
            return lv_color_make(0x22, 0xC5, 0x5E);
        case P4_DESKTOP_ENTRY_SETTINGS:
            return lv_color_make(0x3B, 0x82, 0xF6);
        case P4_DESKTOP_ENTRY_WIFI:
            return lv_color_make(0x14, 0xB8, 0xA6);
        case P4_DESKTOP_ENTRY_DEVICE_INFO:
            return lv_color_make(0x8B, 0x5C, 0xF6);
        case P4_DESKTOP_ENTRY_DIAGNOSTICS:
            return lv_color_make(0xF5, 0x9E, 0x0B);
        case P4_DESKTOP_ENTRY_SENSOR:
            return lv_color_make(0x10, 0xB9, 0x81);
        case P4_DESKTOP_ENTRY_OTA:
            return lv_color_make(0x06, 0xB6, 0xD4);
        case P4_DESKTOP_ENTRY_CAMERA:
            return lv_color_make(0x64, 0x74, 0x8B);
        default:
            return lv_color_make(0x22, 0x26, 0x2E);
    }
}

static void P4DesktopCreateIcon(lv_obj_t *parent, P4DesktopIconType icon, lv_color_t accent)
{
    lv_color_t ink = lv_color_make(0x20, 0x27, 0x30);
    lv_obj_t *a;
    lv_obj_t *b;
    lv_obj_t *c;

    switch (icon) {
        case P4_DESKTOP_ICON_CHAT:
            a = P4DesktopCreateRect(parent, 16, 16, 48, 34, accent, 16);
            b = P4DesktopCreateRect(parent, 28, 47, 14, 12, accent, 4);
            lv_obj_set_style_transform_rotation(b, 450, LV_PART_MAIN);
            c = P4DesktopCreateRect(a, 13, 12, 22, 6, lv_color_white(), LV_RADIUS_CIRCLE);
            (void)c;
            break;
        case P4_DESKTOP_ICON_SETTINGS:
            a = lv_arc_create(parent);
            lv_obj_set_pos(a, 18, 12);
            lv_obj_set_size(a, 44, 44);
            P4DesktopMakeIconObj(a);
            lv_obj_remove_style(a, NULL, LV_PART_KNOB);
            lv_obj_set_style_arc_width(a, 7, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(a, accent, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_arc_set_angles(a, 25, 320);
            b = P4DesktopCreateRect(parent, 32, 26, 16, 16, ink, LV_RADIUS_CIRCLE);
            (void)b;
            break;
        case P4_DESKTOP_ICON_WIFI:
            a = lv_arc_create(parent);
            lv_obj_set_pos(a, 12, 8);
            lv_obj_set_size(a, 56, 56);
            P4DesktopMakeIconObj(a);
            lv_obj_remove_style(a, NULL, LV_PART_KNOB);
            lv_obj_set_style_arc_width(a, 6, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(a, accent, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_arc_set_angles(a, 215, 325);
            b = lv_arc_create(parent);
            lv_obj_set_pos(b, 22, 24);
            lv_obj_set_size(b, 36, 36);
            P4DesktopMakeIconObj(b);
            lv_obj_remove_style(b, NULL, LV_PART_KNOB);
            lv_obj_set_style_arc_width(b, 6, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(b, accent, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(b, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_arc_set_angles(b, 220, 320);
            c = P4DesktopCreateRect(parent, 36, 51, 8, 8, ink, LV_RADIUS_CIRCLE);
            (void)c;
            break;
        case P4_DESKTOP_ICON_INFO:
            a = P4DesktopCreateRect(parent, 20, 10, 40, 50, accent, 8);
            b = P4DesktopCreateRect(a, 14, 10, 12, 12, lv_color_white(), LV_RADIUS_CIRCLE);
            c = P4DesktopCreateRect(a, 17, 26, 6, 16, lv_color_white(), LV_RADIUS_CIRCLE);
            (void)b;
            (void)c;
            break;
        case P4_DESKTOP_ICON_DIAG:
            a = P4DesktopCreateRect(parent, 14, 22, 52, 10, accent, LV_RADIUS_CIRCLE);
            b = P4DesktopCreateRect(parent, 22, 40, 36, 10, accent, LV_RADIUS_CIRCLE);
            c = P4DesktopCreateRect(parent, 25, 11, 30, 30, ink, LV_RADIUS_CIRCLE);
            lv_obj_set_style_bg_opa(c, LV_OPA_30, LV_PART_MAIN);
            break;
        case P4_DESKTOP_ICON_SENSOR:
            a = P4DesktopCreateRect(parent, 27, 10, 18, 44, accent, 9);
            b = P4DesktopCreateRect(parent, 21, 45, 30, 22, accent, LV_RADIUS_CIRCLE);
            c = P4DesktopCreateRect(a, 6, 8, 6, 29, lv_color_white(), LV_RADIUS_CIRCLE);
            P4DesktopCreateRect(parent, 51, 18, 17, 17, lv_color_make(0x38, 0xB2, 0xDF), LV_RADIUS_CIRCLE);
            P4DesktopCreateRect(parent, 55, 43, 9, 9, lv_color_make(0x38, 0xB2, 0xDF), LV_RADIUS_CIRCLE);
            (void)b;
            (void)c;
            break;
        case P4_DESKTOP_ICON_OTA:
            a = P4DesktopCreateRect(parent, 34, 10, 12, 34, accent, 6);
            b = P4DesktopCreateRect(parent, 24, 20, 32, 12, accent, 4);
            c = P4DesktopCreateRect(parent, 20, 50, 40, 8, ink, 4);
            (void)a;
            (void)b;
            (void)c;
            break;
        case P4_DESKTOP_ICON_CAMERA:
        default:
            a = P4DesktopCreateRect(parent, 12, 20, 56, 36, accent, 8);
            b = P4DesktopCreateRect(parent, 22, 12, 18, 12, accent, 5);
            c = P4DesktopCreateRect(a, 18, 8, 20, 20, lv_color_white(), LV_RADIUS_CIRCLE);
            P4DesktopCreateRect(c, 5, 5, 10, 10, ink, LV_RADIUS_CIRCLE);
            (void)b;
            break;
    }
}

static void P4DesktopEventCb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    uintptr_t entry = (uintptr_t)lv_event_get_user_data(e);
    if (entry >= P4_DESKTOP_ENTRY_COUNT) {
        return;
    }

    s_focus_index = (uint32_t)entry;
    P4DesktopPageRefresh();
    P4DesktopPageEnterFocused();
}

static lv_obj_t *P4DesktopCreateLabel(lv_obj_t *parent,
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

static void P4DesktopCreateCard(lv_obj_t *parent, uint32_t idx)
{
    const int card_w = 208;
    const int card_h = 118;
    const int start_x = 24;
    const int start_y = 132;
    const int gap_x = 16;
    const int gap_y = 20;
    const int col = (int)(idx % 2U);
    const int row = (int)(idx / 2U);
    const int x = start_x + col * (card_w + gap_x);
    const int y = start_y + row * (card_h + gap_y);
    const P4DesktopItemDef *def = &s_item_defs[idx];
    P4DesktopItemView *view = &s_items[idx];
    lv_color_t accent = P4DesktopEntryAccent(def->entry);

    view->card = lv_button_create(parent);
    lv_obj_set_pos(view->card, x, y);
    lv_obj_set_size(view->card, card_w, card_h);
    lv_obj_set_style_radius(view->card, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(view->card, lv_color_make(0xD9, 0xE2, 0xEC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view->card, lv_color_make(0xF8, 0xFA, 0xFC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(view->card, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(view->card, P4DesktopEventCb, LV_EVENT_CLICKED, (void *)(uintptr_t)def->entry);

    view->icon_root = lv_obj_create(view->card);
    lv_obj_set_pos(view->icon_root, 10, 13);
    lv_obj_set_size(view->icon_root, 80, 72);
    lv_obj_set_style_bg_opa(view->icon_root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->icon_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(view->icon_root, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(view->icon_root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(view->icon_root, LV_OBJ_FLAG_SCROLLABLE);
    P4DesktopCreateIcon(view->icon_root, def->icon, accent);

    view->title = P4DesktopCreateLabel(view->card,
                                       def->title,
                                       96,
                                       20,
                                       98,
                                       28,
                                       lv_color_make(0x1F, 0x29, 0x37),
                                       LV_TEXT_ALIGN_LEFT);
    view->subtitle = P4DesktopCreateLabel(view->card,
                                          def->subtitle,
                                          96,
                                          52,
                                          98,
                                          24,
                                          lv_color_make(0x64, 0x74, 0x8B),
                                          LV_TEXT_ALIGN_LEFT);
    view->badge = P4DesktopCreateLabel(view->card,
                                       "--",
                                       96,
                                       82,
                                       98,
                                       24,
                                       accent,
                                       LV_TEXT_ALIGN_LEFT);
}

static void P4DesktopApplyFocus(uint32_t focus)
{
    for (uint32_t i = 0; i < P4_DESKTOP_ENTRY_COUNT; ++i) {
        P4DesktopItemView *view = &s_items[i];
        if (view->card == NULL || !lv_obj_is_valid(view->card)) {
            continue;
        }

        if (i == focus) {
            lv_obj_set_style_border_width(view->card, 3, LV_PART_MAIN);
            lv_obj_set_style_border_color(view->card, P4DesktopEntryAccent(s_item_defs[i].entry), LV_PART_MAIN);
            lv_obj_set_style_bg_color(view->card, lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(view->card, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(view->card, lv_color_make(0xD9, 0xE2, 0xEC), LV_PART_MAIN);
            lv_obj_set_style_bg_color(view->card, lv_color_make(0xF8, 0xFA, 0xFC), LV_PART_MAIN);
        }
    }
}

void P4DesktopPageHide(void)
{
    s_visible = 0U;
    s_status_label = NULL;
    s_state_label = NULL;
    s_hint_label = NULL;
    s_footer_label = NULL;
    (void)memset(s_items, 0, sizeof(s_items));
}

void P4DesktopPageRefresh(void)
{
    P4UiModelSnapshot model;
    char state_text[96];
    char footer[128];

    if (!s_visible || s_state_label == NULL || !lv_obj_is_valid(s_state_label)) {
        return;
    }

    P4UiModelGetSnapshot(&model);

    if (s_status_label != NULL && lv_obj_is_valid(s_status_label)) {
        P4StatusBarUpdate(s_status_label, &model);
    }

    (void)snprintf(state_text,
                   sizeof(state_text),
                   "%s | %s",
                   P4UiModelDialogStateText(model.dialog_state),
                   model.uart.link_ready ? "WS63 已连接" : "WS63 等待连接");
    P4DesktopSetText(s_state_label, state_text);

    for (uint32_t i = 0; i < P4_DESKTOP_ENTRY_COUNT; ++i) {
        if (s_items[i].badge != NULL && lv_obj_is_valid(s_items[i].badge)) {
            P4DesktopSetText(s_items[i].badge, P4DesktopEntryBadge(s_item_defs[i].entry, &model));
        }
    }

    P4DesktopApplyFocus(s_focus_index);

    (void)snprintf(footer,
                   sizeof(footer),
                   "当前：%s | 触摸进入 | K4 对话/桌面 K5/K6 选择 K7 进入",
                   s_item_defs[s_focus_index].title);
    P4DesktopSetText(s_footer_label, footer);
}

void P4DesktopPageShow(const P4DesktopPageOps *ops)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *title;
    lv_obj_t *band;

    if (scr == NULL) {
        return;
    }

    P4DesktopPageHide();
    (void)memset(&s_ops, 0, sizeof(s_ops));
    if (ops != NULL) {
        s_ops = *ops;
    }

    lv_obj_clean(scr);
    P4SystemFontApply(scr);
    lv_obj_set_style_bg_color(scr, lv_color_make(0xF1, 0xF5, 0xF9), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    s_status_label = P4StatusBarCreate(scr);

    title = P4DesktopCreateLabel(scr,
                                 "小鸿 SE",
                                 24,
                                 58,
                                 180,
                                 34,
                                 lv_color_make(0x17, 0x24, 0x33),
                                 LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_font(title, P4SystemFontGet(), LV_PART_MAIN);

    s_state_label = P4DesktopCreateLabel(scr,
                                         "状态同步中",
                                         204,
                                         60,
                                         252,
                                         30,
                                         lv_color_make(0x47, 0x55, 0x69),
                                         LV_TEXT_ALIGN_RIGHT);

    band = P4DesktopCreateRect(scr, 24, 102, 432, 14, lv_color_make(0xD8, 0xEC, 0xFF), 7);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, LV_PART_MAIN);

    for (uint32_t i = 0; i < P4_DESKTOP_ENTRY_COUNT; ++i) {
        P4DesktopCreateCard(scr, i);
    }

    s_hint_label = P4DesktopCreateLabel(scr,
                                        "产品主菜单",
                                        24,
                                        692,
                                        432,
                                        28,
                                        lv_color_make(0x33, 0x41, 0x55),
                                        LV_TEXT_ALIGN_CENTER);
    (void)s_hint_label;

    s_footer_label = P4DesktopCreateLabel(scr,
                                          "触摸进入 | K4 对话/桌面 K5/K6 选择 K7 进入",
                                          20,
                                          736,
                                          440,
                                          36,
                                          lv_color_make(0x64, 0x74, 0x8B),
                                          LV_TEXT_ALIGN_CENTER);

    s_visible = 1U;
    P4DesktopPageRefresh();

    ESP_LOGI(TAG, "desktop page created entries=%u", (unsigned)P4_DESKTOP_ENTRY_COUNT);
}

void P4DesktopPageMoveFocus(int delta)
{
    int next;

    if (!s_visible) {
        return;
    }

    next = (int)s_focus_index + delta;
    while (next < 0) {
        next += (int)P4_DESKTOP_ENTRY_COUNT;
    }
    next %= (int)P4_DESKTOP_ENTRY_COUNT;
    s_focus_index = (uint32_t)next;
    P4DesktopPageRefresh();
}

void P4DesktopPageEnterFocused(void)
{
    P4DesktopEntry entry;

    if (!s_visible || s_focus_index >= P4_DESKTOP_ENTRY_COUNT) {
        return;
    }

    entry = s_item_defs[s_focus_index].entry;
    ESP_LOGI(TAG, "desktop entry selected entry=%u title=%s", (unsigned)entry, s_item_defs[s_focus_index].title);

    if (s_ops.on_entry != NULL) {
        s_ops.on_entry(entry);
    }
}

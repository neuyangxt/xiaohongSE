#include "p4_dialog_page.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "ohos_uart_link_ui.h"
#include "p4_status_bar.h"
#include "p4_system_font.h"
#include "p4_text_view.h"
#include "p4_ui_model.h"

static const char *TAG = "p4_dialog";

typedef struct {
    lv_obj_t *root;
    lv_obj_t *head;
    lv_obj_t *left_eye;
    lv_obj_t *right_eye;
    lv_obj_t *mouth_arc;
    lv_obj_t *mouth_bar;
    lv_obj_t *left_cheek;
    lv_obj_t *right_cheek;
    lv_obj_t *voice_bar_1;
    lv_obj_t *voice_bar_2;
    lv_obj_t *voice_bar_3;
} P4DialogFaceView;

static P4DialogPageBackHandler s_back_handler;
static lv_obj_t *s_status_label;
static P4DialogFaceView s_face;
static lv_obj_t *s_prompt_label;
static lv_obj_t *s_text_label;
static lv_obj_t *s_bottom_label;

static uint32_t s_last_down_text_count = 0xFFFFFFFFU;
static uint32_t s_last_down_text_type = 0xFFFFFFFFU;
static uint32_t s_last_agent_status = 0xFFFFFFFFU;
static uint32_t s_last_link_ready = 0xFFFFFFFFU;
static uint32_t s_last_up_opus_running = 0xFFFFFFFFU;
static uint32_t s_last_down_audio_stream_open = 0xFFFFFFFFU;
static uint32_t s_last_interrupt_mode = 0xFFFFFFFFU;
static uint32_t s_last_volume = 0xFFFFFFFFU;
static uint32_t s_last_brightness = 0xFFFFFFFFU;
static uint32_t s_invalid_baseline;
static uint32_t s_first_refresh;
static uint32_t s_dialog_visible;
static uint32_t s_last_face_state = 0xFFFFFFFFU;
static volatile uint32_t s_dirty;

static char s_prompt_cache[64];
static char s_bottom_cache[192];
static char s_dialog_text_cache[896];
static char s_down_text_raw[384];
static char s_down_text_formatted[896];

static void P4DialogSetLabelText(lv_obj_t *label, char *cache, size_t cache_len, const char *text)
{
    if (label == NULL || cache == NULL || cache_len == 0U || text == NULL || !lv_obj_is_valid(label)) {
        return;
    }

    if (strncmp(cache, text, cache_len) == 0) {
        return;
    }

    (void)snprintf(cache, cache_len, "%s", text);
    lv_label_set_text(label, cache);
}

static void P4DialogApplyDialogFont(lv_obj_t *label)
{
    if (label == NULL || !lv_obj_is_valid(label)) {
        return;
    }

    lv_obj_set_style_text_font(label, P4SystemFontGet(), LV_PART_MAIN);
}

static lv_obj_t *P4DialogCreatePlainLabel(lv_obj_t *parent,
                                          int x,
                                          int y,
                                          int w,
                                          int h,
                                          lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, P4SystemFontGet(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_make(0x21, 0x25, 0x2B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);

    return label;
}

static void P4DialogSetObjHidden(lv_obj_t *obj, uint32_t hidden)
{
    if (obj == NULL || !lv_obj_is_valid(obj)) {
        return;
    }

    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *P4DialogCreateFacePart(lv_obj_t *parent,
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

    return obj;
}

static void P4DialogFaceSetPart(lv_obj_t *obj,
                                int x,
                                int y,
                                int w,
                                int h,
                                lv_color_t color,
                                int radius)
{
    if (obj == NULL || !lv_obj_is_valid(obj)) {
        return;
    }

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
}

static void P4DialogCreateFace(lv_obj_t *parent, int x, int y, int w, int h)
{
    const lv_color_t face_bg = lv_color_make(0xFE, 0xE6, 0x8A);
    const lv_color_t face_border = lv_color_make(0xF5, 0xB8, 0x44);
    const lv_color_t ink = lv_color_make(0x24, 0x2A, 0x33);
    const int head_size = 156;
    const int head_x = (w - head_size) / 2;

    (void)memset(&s_face, 0, sizeof(s_face));

    s_face.root = lv_obj_create(parent);
    lv_obj_set_pos(s_face.root, x, y);
    lv_obj_set_size(s_face.root, w, h);
    lv_obj_set_style_bg_opa(s_face.root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_face.root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_face.root, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_face.root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_face.root, LV_OBJ_FLAG_SCROLLABLE);

    s_face.head = P4DialogCreateFacePart(s_face.root, head_x, 8, head_size, head_size, face_bg, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(s_face.head, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_face.head, face_border, LV_PART_MAIN);

    s_face.left_eye = P4DialogCreateFacePart(s_face.head, 45, 55, 18, 24, ink, LV_RADIUS_CIRCLE);
    s_face.right_eye = P4DialogCreateFacePart(s_face.head, 93, 55, 18, 24, ink, LV_RADIUS_CIRCLE);
    s_face.left_cheek = P4DialogCreateFacePart(s_face.head, 28, 86, 24, 12, lv_color_make(0xFB, 0xA5, 0xA5), LV_RADIUS_CIRCLE);
    s_face.right_cheek = P4DialogCreateFacePart(s_face.head, 104, 86, 24, 12, lv_color_make(0xFB, 0xA5, 0xA5), LV_RADIUS_CIRCLE);

    s_face.mouth_arc = lv_arc_create(s_face.head);
    lv_obj_set_pos(s_face.mouth_arc, 40, 76);
    lv_obj_set_size(s_face.mouth_arc, 76, 58);
    lv_obj_remove_style(s_face.mouth_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_opa(s_face.mouth_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_face.mouth_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_face.mouth_arc, ink, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_face.mouth_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_arc_set_bg_angles(s_face.mouth_arc, 0, 360);
    lv_arc_set_angles(s_face.mouth_arc, 35, 145);

    s_face.mouth_bar = P4DialogCreateFacePart(s_face.head, 66, 96, 24, 12, ink, LV_RADIUS_CIRCLE);
    P4DialogSetObjHidden(s_face.mouth_bar, 1U);

    s_face.voice_bar_1 = P4DialogCreateFacePart(s_face.head, 119, 85, 7, 28, lv_color_make(0x22, 0xC5, 0x5E), LV_RADIUS_CIRCLE);
    s_face.voice_bar_2 = P4DialogCreateFacePart(s_face.head, 132, 76, 7, 46, lv_color_make(0x22, 0xC5, 0x5E), LV_RADIUS_CIRCLE);
    s_face.voice_bar_3 = P4DialogCreateFacePart(s_face.head, 145, 91, 7, 20, lv_color_make(0x22, 0xC5, 0x5E), LV_RADIUS_CIRCLE);
    P4DialogSetObjHidden(s_face.voice_bar_1, 1U);
    P4DialogSetObjHidden(s_face.voice_bar_2, 1U);
    P4DialogSetObjHidden(s_face.voice_bar_3, 1U);
}

static void P4DialogApplyFaceState(P4UiDialogState state)
{
    lv_color_t face_bg = lv_color_make(0xFE, 0xE6, 0x8A);
    lv_color_t face_border = lv_color_make(0xF5, 0xB8, 0x44);
    lv_color_t ink = lv_color_make(0x24, 0x2A, 0x33);
    lv_color_t cheek = lv_color_make(0xFB, 0xA5, 0xA5);
    lv_color_t voice = lv_color_make(0x22, 0xC5, 0x5E);

    if (s_face.root == NULL || !lv_obj_is_valid(s_face.root)) {
        return;
    }

    if (s_last_face_state == (uint32_t)state) {
        return;
    }
    s_last_face_state = (uint32_t)state;

    if (state == P4_UI_DIALOG_STATE_IDLE) {
        face_bg = lv_color_make(0xDB, 0xEA, 0xFE);
        face_border = lv_color_make(0x60, 0xA5, 0xFA);
        cheek = lv_color_make(0xBA, 0xE6, 0xFD);
    } else if (state == P4_UI_DIALOG_STATE_LISTENING) {
        face_bg = lv_color_make(0xCC, 0xFB, 0xF1);
        face_border = lv_color_make(0x14, 0xB8, 0xA6);
        cheek = lv_color_make(0x99, 0xF6, 0xE4);
    } else if (state == P4_UI_DIALOG_STATE_SPEAKING) {
        face_bg = lv_color_make(0xDC, 0xFC, 0xE7);
        face_border = lv_color_make(0x22, 0xC5, 0x5E);
        cheek = lv_color_make(0xBB, 0xF7, 0xD0);
    } else if (state == P4_UI_DIALOG_STATE_ERROR) {
        face_bg = lv_color_make(0xFE, 0xE2, 0xE2);
        face_border = lv_color_make(0xEF, 0x44, 0x44);
        ink = lv_color_make(0xB9, 0x1C, 0x1C);
        cheek = lv_color_make(0xFE, 0xCA, 0xCA);
        voice = lv_color_make(0xEF, 0x44, 0x44);
    }

    lv_obj_set_style_bg_color(s_face.head, face_bg, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_face.head, face_border, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_face.mouth_arc, ink, LV_PART_INDICATOR);
    P4DialogFaceSetPart(s_face.left_cheek, 28, 86, 24, 12, cheek, LV_RADIUS_CIRCLE);
    P4DialogFaceSetPart(s_face.right_cheek, 104, 86, 24, 12, cheek, LV_RADIUS_CIRCLE);

    P4DialogFaceSetPart(s_face.left_eye, 45, 55, 18, 24, ink, LV_RADIUS_CIRCLE);
    P4DialogFaceSetPart(s_face.right_eye, 93, 55, 18, 24, ink, LV_RADIUS_CIRCLE);
    P4DialogSetObjHidden(s_face.left_cheek, 0U);
    P4DialogSetObjHidden(s_face.right_cheek, 0U);
    P4DialogSetObjHidden(s_face.mouth_arc, 0U);
    P4DialogSetObjHidden(s_face.mouth_bar, 1U);
    P4DialogSetObjHidden(s_face.voice_bar_1, 1U);
    P4DialogSetObjHidden(s_face.voice_bar_2, 1U);
    P4DialogSetObjHidden(s_face.voice_bar_3, 1U);
    lv_arc_set_angles(s_face.mouth_arc, 35, 145);

    switch (state) {
        case P4_UI_DIALOG_STATE_CONNECTING:
        case P4_UI_DIALOG_STATE_UNKNOWN:
        case P4_UI_DIALOG_STATE_WAKEUP:
            P4DialogFaceSetPart(s_face.left_eye, 42, 64, 28, 7, ink, LV_RADIUS_CIRCLE);
            P4DialogFaceSetPart(s_face.right_eye, 94, 55, 18, 24, ink, LV_RADIUS_CIRCLE);
            break;
        case P4_UI_DIALOG_STATE_IDLE:
            P4DialogFaceSetPart(s_face.left_eye, 42, 64, 28, 7, ink, LV_RADIUS_CIRCLE);
            P4DialogFaceSetPart(s_face.right_eye, 86, 64, 28, 7, ink, LV_RADIUS_CIRCLE);
            P4DialogSetObjHidden(s_face.mouth_arc, 1U);
            P4DialogSetObjHidden(s_face.mouth_bar, 0U);
            P4DialogFaceSetPart(s_face.mouth_bar, 66, 96, 24, 12, ink, LV_RADIUS_CIRCLE);
            break;
        case P4_UI_DIALOG_STATE_LISTENING:
            lv_arc_set_angles(s_face.mouth_arc, 25, 155);
            break;
        case P4_UI_DIALOG_STATE_SPEAKING:
            P4DialogSetObjHidden(s_face.mouth_arc, 1U);
            P4DialogSetObjHidden(s_face.mouth_bar, 0U);
            P4DialogFaceSetPart(s_face.mouth_bar, 61, 88, 34, 40, ink, LV_RADIUS_CIRCLE);
            P4DialogFaceSetPart(s_face.voice_bar_1, 119, 85, 7, 28, voice, LV_RADIUS_CIRCLE);
            P4DialogFaceSetPart(s_face.voice_bar_2, 132, 76, 7, 46, voice, LV_RADIUS_CIRCLE);
            P4DialogFaceSetPart(s_face.voice_bar_3, 145, 91, 7, 20, voice, LV_RADIUS_CIRCLE);
            P4DialogSetObjHidden(s_face.voice_bar_1, 0U);
            P4DialogSetObjHidden(s_face.voice_bar_2, 0U);
            P4DialogSetObjHidden(s_face.voice_bar_3, 0U);
            break;
        case P4_UI_DIALOG_STATE_ERROR:
        default:
            P4DialogFaceSetPart(s_face.left_eye, 42, 60, 28, 7, ink, LV_RADIUS_CIRCLE);
            P4DialogFaceSetPart(s_face.right_eye, 86, 60, 28, 7, ink, LV_RADIUS_CIRCLE);
            P4DialogSetObjHidden(s_face.left_cheek, 1U);
            P4DialogSetObjHidden(s_face.right_cheek, 1U);
            lv_arc_set_angles(s_face.mouth_arc, 215, 325);
            break;
    }
}


static void P4DialogBackEvent(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ESP_LOGI(TAG, "Dialog back clicked");
    P4DialogPageHide();
    if (s_back_handler != NULL) {
        s_back_handler();
    }
}

static void P4DialogWakeUpEvent(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ESP_LOGI(TAG, "Dialog interrupt clicked");
    (void)OhosUartLinkUiVoiceUserAction();
}

void P4DialogPageSetBackHandler(P4DialogPageBackHandler handler)
{
    s_back_handler = handler;
}

void P4DialogPageHide(void)
{
    s_dialog_visible = 0U;
    s_status_label = NULL;
    (void)memset(&s_face, 0, sizeof(s_face));
    s_prompt_label = NULL;
    s_text_label = NULL;
    s_bottom_label = NULL;
}

void P4DialogPageCreate(lv_obj_t *parent)
{
    lv_obj_t *text_panel;
    lv_obj_t *wakeup_btn;
    lv_obj_t *wakeup_label;
    lv_obj_t *back_btn;
    lv_obj_t *back_label;
    lv_obj_t *hint_panel;

    if (parent == NULL || !lv_obj_is_valid(parent)) {
        return;
    }

    P4SystemFontApply(parent);

    s_status_label = P4StatusBarCreate(parent);

    P4DialogCreateFace(parent, 16, 66, 448, 210);

    s_prompt_label = P4DialogCreatePlainLabel(parent, 16, 292, 448, 44, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text(s_prompt_label, "状态同步中");

    text_panel = lv_obj_create(parent);
    lv_obj_set_pos(text_panel, 24, 356);
    lv_obj_set_size(text_panel, 432, 240);
    lv_obj_set_style_radius(text_panel, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(text_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(text_panel, lv_color_make(0xDD, 0xE2, 0xEA), LV_PART_MAIN);
    lv_obj_set_style_bg_color(text_panel, lv_color_make(0xF8, 0xFA, 0xFC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(text_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(text_panel, 12, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(text_panel, LV_SCROLLBAR_MODE_OFF);

    s_text_label = lv_label_create(text_panel);
    lv_obj_set_width(s_text_label, 408);
    lv_label_set_long_mode(s_text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_text_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_text_label, lv_color_make(0x1F, 0x29, 0x37), LV_PART_MAIN);
    P4DialogApplyDialogFont(s_text_label);
    lv_label_set_text(s_text_label, "等待对话文本");

    {
        const int scr_w = 480;
        const int center_x = scr_w / 2;
        const int btn_w = 140;
        const int btn_h = 54;
        const int btn_y = 656;
        const int btn_gap = 24;
        const int hint_w = 408;
        const int hint_h = 54;
        const int hint_y = 710;

        wakeup_btn = lv_button_create(parent);
        lv_obj_set_pos(wakeup_btn, center_x - btn_gap - btn_w, btn_y);
        lv_obj_set_size(wakeup_btn, btn_w, btn_h);
        lv_obj_add_event_cb(wakeup_btn, P4DialogWakeUpEvent, LV_EVENT_CLICKED, NULL);

        wakeup_label = lv_label_create(wakeup_btn);
        lv_label_set_text(wakeup_label, "唤醒");
        lv_obj_set_style_text_font(wakeup_label, P4SystemFontGet(), LV_PART_MAIN);
        lv_obj_center(wakeup_label);

        back_btn = lv_button_create(parent);
        lv_obj_set_pos(back_btn, center_x + btn_gap, btn_y);
        lv_obj_set_size(back_btn, btn_w, btn_h);
        lv_obj_add_event_cb(back_btn, P4DialogBackEvent, LV_EVENT_CLICKED, NULL);

        back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, "返回");
        lv_obj_center(back_label);

        hint_panel = lv_obj_create(parent);
        lv_obj_set_pos(hint_panel, (scr_w - hint_w) / 2, hint_y);
        lv_obj_set_size(hint_panel, hint_w, hint_h);
        lv_obj_set_style_radius(hint_panel, 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(hint_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(hint_panel, lv_color_make(0xEF, 0xF3, 0xF8), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(hint_panel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(hint_panel, 8, LV_PART_MAIN);
        lv_obj_set_scrollbar_mode(hint_panel, LV_SCROLLBAR_MODE_OFF);

        s_bottom_label = lv_label_create(hint_panel);
        lv_obj_set_width(s_bottom_label, hint_w - 16);
        lv_label_set_long_mode(s_bottom_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(s_bottom_label, P4SystemFontGet(), LV_PART_MAIN);
        lv_obj_set_style_text_align(s_bottom_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(s_bottom_label);
        lv_label_set_text(s_bottom_label, "Agent:Unknown | INT:OFF");
    }
}

static void P4DialogRefreshText(const P4UiModelSnapshot *model)
{
    if (model == NULL || s_text_label == NULL || !lv_obj_is_valid(s_text_label)) {
        return;
    }

    if (model->uart.down_text_count == s_last_down_text_count &&
        model->uart.down_text_type == s_last_down_text_type) {
        return;
    }

    s_down_text_raw[0] = '\0';
    OhosUartLinkUiGetDownText(s_down_text_raw, sizeof(s_down_text_raw));
    P4TextFormatDownText(model->uart.down_text_type,
                         s_down_text_raw,
                         s_down_text_formatted,
                         sizeof(s_down_text_formatted));
    P4DialogSetLabelText(s_text_label,
                         s_dialog_text_cache,
                         sizeof(s_dialog_text_cache),
                         s_down_text_formatted);

    s_last_down_text_count = model->uart.down_text_count;
    s_last_down_text_type = model->uart.down_text_type;
}

void P4DialogPageRefresh(void)
{
    P4UiModelSnapshot model;
    P4UiDialogState state;
    char bottom[192];

    if (s_status_label == NULL || s_face.root == NULL || s_prompt_label == NULL ||
        s_text_label == NULL || s_bottom_label == NULL) {
        return;
    }

    P4UiModelGetSnapshot(&model);
    state = model.dialog_state;

    if (s_first_refresh) {
        s_invalid_baseline = model.uart.agent_status_invalid_count;
        s_first_refresh = 0U;
    } else if (model.uart.agent_status_invalid_count > s_invalid_baseline) {
        state = P4_UI_DIALOG_STATE_ERROR;
    }

    P4StatusBarUpdate(s_status_label, &model);
    P4DialogApplyFaceState(state);
    P4DialogSetLabelText(s_prompt_label,
                         s_prompt_cache,
                         sizeof(s_prompt_cache),
                         P4UiModelDialogStateText(state));

    (void)snprintf(bottom,
                   sizeof(bottom),
                   "Agent:%s | Up:%s | Down:%s",
                   P4UiModelAgentStatusName(model.uart.agent_status),
                   model.uart.up_opus_running ? "RUN" : "STOP",
                   model.uart.down_audio_stream_open ? "PLAY" : "IDLE");
    P4DialogSetLabelText(s_bottom_label, s_bottom_cache, sizeof(s_bottom_cache), bottom);

    P4DialogRefreshText(&model);

    s_last_agent_status = model.uart.agent_status;
    s_last_link_ready = model.uart.link_ready;
    s_last_up_opus_running = model.uart.up_opus_running;
    s_last_down_audio_stream_open = model.uart.down_audio_stream_open;
    s_last_interrupt_mode = model.interrupt_mode;
    s_last_volume = model.volume;
    s_last_brightness = model.brightness;
}

void P4DialogPageMarkDirty(void)
{
    s_dirty = 1U;
}

void P4DialogPageProcessPending(void)
{
    if (!s_dialog_visible || s_status_label == NULL || !lv_obj_is_valid(s_status_label)) {
        return;
    }

    if (!s_dirty) {
        return;
    }
    s_dirty = 0U;

    P4DialogPageRefresh();
}

void P4DialogPageShow(void)
{
    P4UiModelSnapshot model;
    lv_obj_t *scr = lv_screen_active();

    if (scr == NULL) {
        return;
    }

    P4DialogPageHide();
    (void)memset(s_prompt_cache, 0, sizeof(s_prompt_cache));
    (void)memset(s_bottom_cache, 0, sizeof(s_bottom_cache));
    (void)memset(s_dialog_text_cache, 0, sizeof(s_dialog_text_cache));
    (void)memset(s_down_text_raw, 0, sizeof(s_down_text_raw));
    (void)memset(s_down_text_formatted, 0, sizeof(s_down_text_formatted));
    s_last_down_text_count = 0xFFFFFFFFU;
    s_last_down_text_type = 0xFFFFFFFFU;
    s_last_agent_status = 0xFFFFFFFFU;
    s_last_link_ready = 0xFFFFFFFFU;
    s_last_up_opus_running = 0xFFFFFFFFU;
    s_last_down_audio_stream_open = 0xFFFFFFFFU;
    s_last_interrupt_mode = 0xFFFFFFFFU;
    s_last_volume = 0xFFFFFFFFU;
    s_last_brightness = 0xFFFFFFFFU;
    s_invalid_baseline = 0U;
    s_first_refresh = 1U;
    s_last_face_state = 0xFFFFFFFFU;
    s_dirty = 0U;

    lv_obj_clean(scr);
    P4DialogPageCreate(scr);
    s_dialog_visible = 1U;

    P4UiModelGetSnapshot(&model);
    if (model.uart.link_ready && model.uart.agent_status == 0xFFFFFFFFU) {
        (void)OhosUartLinkUiQueryAgentStatus();
    }

    P4DialogPageRefresh();

    ESP_LOGI(TAG, "Dialog page created refresh=event-only external_font=off face=lvgl_static");
}

void P4DialogPageOnDownText(uint32_t type, const char *utf8)
{
    if (s_text_label == NULL || !lv_obj_is_valid(s_text_label)) {
        return;
    }

    P4TextFormatDownText(type, utf8, s_down_text_formatted, sizeof(s_down_text_formatted));
    P4DialogSetLabelText(s_text_label,
                         s_dialog_text_cache,
                         sizeof(s_dialog_text_cache),
                         s_down_text_formatted);
}

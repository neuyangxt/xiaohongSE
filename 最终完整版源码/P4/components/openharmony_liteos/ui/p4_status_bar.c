#include "p4_status_bar.h"

#include <stdio.h>
#include <string.h>

#include "p4_system_font.h"

static char s_status_bar_cache[160];

lv_obj_t *P4StatusBarCreate(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_pos(label, 16, 10);
    lv_obj_set_size(label, 448, 38);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, P4SystemFontGet(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_make(0x22, 0x26, 0x2E), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(label, "WS63:WAIT Agent:Unknown INT:OFF V:60 B:70");
    s_status_bar_cache[0] = '\0';

    return label;
}

void P4StatusBarUpdate(lv_obj_t *label, const P4UiModelSnapshot *model)
{
    char text[160];
    const char *link;
    const char *agent;
    const char *interrupt_mode;

    if (label == NULL || model == NULL || !lv_obj_is_valid(label)) {
        return;
    }

    link = model->uart.link_ready ? "READY" : "WAIT";
    agent = P4UiModelAgentStatusName(model->uart.agent_status);
    interrupt_mode = model->interrupt_mode ? "ON" : "OFF";

    (void)snprintf(text,
                   sizeof(text),
                   "WS63:%s Agent:%s INT:%s V:%u B:%u",
                   link,
                   agent,
                   interrupt_mode,
                   (unsigned)model->volume,
                   (unsigned)model->brightness);
    if (strncmp(s_status_bar_cache, text, sizeof(s_status_bar_cache)) == 0) {
        return;
    }
    (void)snprintf(s_status_bar_cache, sizeof(s_status_bar_cache), "%s", text);
    lv_label_set_text(label, text);
}

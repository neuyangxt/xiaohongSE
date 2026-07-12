#include "p4_system_font.h"

#include "esp_rom_sys.h"

LV_FONT_DECLARE(p4_system_font_cn_16);

static uint32_t g_p4_system_font_log_once;

const lv_font_t *P4SystemFontGet(void)
{
    return &p4_system_font_cn_16;
}

const char *P4SystemFontName(void)
{
    return "p4_system_font_cn_16";
}

uint32_t P4SystemFontIsCjkEnabled(void)
{
    return 1U;
}

void P4SystemFontApply(lv_obj_t *obj)
{
    const lv_font_t *font = P4SystemFontGet();

    if (obj == NULL || !lv_obj_is_valid(obj) || font == NULL) {
        return;
    }

    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);

    if (!g_p4_system_font_log_once) {
        g_p4_system_font_log_once = 1U;
        esp_rom_printf("[P4-SYSFONT] apply name=%s cjk=%u line_height=%d base_line=%d\n",
                       P4SystemFontName(),
                       (unsigned)P4SystemFontIsCjkEnabled(),
                       (int)font->line_height,
                       (int)font->base_line);
    }
}

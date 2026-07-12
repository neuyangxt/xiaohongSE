#ifndef P4_SYSTEM_FONT_H
#define P4_SYSTEM_FONT_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

const lv_font_t *P4SystemFontGet(void);
const char *P4SystemFontName(void);
uint32_t P4SystemFontIsCjkEnabled(void);
void P4SystemFontApply(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif

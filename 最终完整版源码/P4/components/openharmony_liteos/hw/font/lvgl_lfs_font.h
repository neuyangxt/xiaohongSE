#ifndef LVGL_LFS_FONT_H
#define LVGL_LFS_FONT_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t OhosLvglLfsFontRegister(void);
uint32_t OhosLvglExternalFontLoad(void);
uint32_t OhosLvglExternalFontIsLoaded(void);
const lv_font_t *OhosLvglExternalFontGet(void);
void OhosLvglExternalFontRelease(void);

#ifdef __cplusplus
}
#endif

#endif

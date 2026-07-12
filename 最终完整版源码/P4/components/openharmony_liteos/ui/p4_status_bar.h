#ifndef P4_STATUS_BAR_H
#define P4_STATUS_BAR_H

#include "lvgl.h"
#include "p4_ui_model.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *P4StatusBarCreate(lv_obj_t *parent);
void P4StatusBarUpdate(lv_obj_t *label, const P4UiModelSnapshot *model);

#ifdef __cplusplus
}
#endif

#endif

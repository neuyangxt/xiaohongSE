#ifndef P4_DIALOG_PAGE_H
#define P4_DIALOG_PAGE_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*P4DialogPageBackHandler)(void);

void P4DialogPageSetBackHandler(P4DialogPageBackHandler handler);
void P4DialogPageCreate(lv_obj_t *parent);
void P4DialogPageShow(void);
void P4DialogPageHide(void);
void P4DialogPageRefresh(void);
void P4DialogPageMarkDirty(void);
void P4DialogPageProcessPending(void);
void P4DialogPageOnDownText(uint32_t type, const char *utf8);

#ifdef __cplusplus
}
#endif

#endif

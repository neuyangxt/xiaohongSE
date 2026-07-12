#ifndef P4_DESKTOP_PAGE_H
#define P4_DESKTOP_PAGE_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    P4_DESKTOP_ENTRY_DIALOG = 0,
    P4_DESKTOP_ENTRY_SETTINGS,
    P4_DESKTOP_ENTRY_WIFI,
    P4_DESKTOP_ENTRY_DEVICE_INFO,
    P4_DESKTOP_ENTRY_DIAGNOSTICS,
    P4_DESKTOP_ENTRY_SENSOR,
    P4_DESKTOP_ENTRY_OTA,
    P4_DESKTOP_ENTRY_CAMERA,
    P4_DESKTOP_ENTRY_COUNT
} P4DesktopEntry;

typedef void (*P4DesktopEntryHandler)(P4DesktopEntry entry);

typedef struct {
    P4DesktopEntryHandler on_entry;
} P4DesktopPageOps;

void P4DesktopPageShow(const P4DesktopPageOps *ops);
void P4DesktopPageHide(void);
void P4DesktopPageRefresh(void);
void P4DesktopPageMoveFocus(int delta);
void P4DesktopPageEnterFocused(void);

#ifdef __cplusplus
}
#endif

#endif

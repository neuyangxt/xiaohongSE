#ifndef P4_SENSOR_PAGE_H
#define P4_SENSOR_PAGE_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*P4SensorPageBackHandler)(void);

void P4SensorPageSetBackHandler(P4SensorPageBackHandler handler);
void P4SensorPageShow(void);
void P4SensorPageHide(void);
void P4SensorPageRefresh(void);
void P4SensorPageProcessPending(void);

#ifdef __cplusplus
}
#endif

#endif

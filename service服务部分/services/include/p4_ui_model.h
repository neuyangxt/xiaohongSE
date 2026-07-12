#ifndef P4_UI_MODEL_H
#define P4_UI_MODEL_H

#include <stdint.h>

#include "ohos_uart_link_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    P4_UI_DIALOG_STATE_CONNECTING = 0,
    P4_UI_DIALOG_STATE_UNKNOWN,
    P4_UI_DIALOG_STATE_IDLE,
    P4_UI_DIALOG_STATE_WAKEUP,
    P4_UI_DIALOG_STATE_LISTENING,
    P4_UI_DIALOG_STATE_SPEAKING,
    P4_UI_DIALOG_STATE_ERROR,
} P4UiDialogState;

typedef OhosUartLinkSensorSnapshot P4UiSensorThSnapshot;

typedef struct {
    OhosUartLinkUiSnapshot uart;
    P4UiSensorThSnapshot sensor_th;
    P4UiDialogState dialog_state;
    uint32_t config_ready;
    uint32_t volume;
    uint32_t brightness;
    uint32_t interrupt_mode;
    uint32_t external_font_loaded;
    uint32_t wifi_link_status;
} P4UiModelSnapshot;

void P4UiModelGetSnapshot(P4UiModelSnapshot *out);
P4UiDialogState P4UiModelMapDialogState(const OhosUartLinkUiSnapshot *snap);
const char *P4UiModelDialogStateText(P4UiDialogState state);
const char *P4UiModelDialogEmojiText(P4UiDialogState state);
const char *P4UiModelAgentStatusName(uint32_t status);

#ifdef __cplusplus
}
#endif

#endif

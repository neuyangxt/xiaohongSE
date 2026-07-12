#include "p4_ui_model.h"

#include <string.h>

#include "config_service.h"
#include "lvgl_lfs_font.h"

P4UiDialogState P4UiModelMapDialogState(const OhosUartLinkUiSnapshot *snap)
{
    uint32_t agent;

    if (snap == NULL) {
        return P4_UI_DIALOG_STATE_UNKNOWN;
    }

    if (!snap->link_ready) {
        return P4_UI_DIALOG_STATE_CONNECTING;
    }

    agent = snap->agent_status;
    if (agent == 0xFFFFFFFFU) {
        return P4_UI_DIALOG_STATE_UNKNOWN;
    }

    switch (agent) {
        case 0U:
            return P4_UI_DIALOG_STATE_IDLE;
        case 1U:
            return P4_UI_DIALOG_STATE_WAKEUP;
        case 2U:
            return P4_UI_DIALOG_STATE_LISTENING;
        case 3U:
            return P4_UI_DIALOG_STATE_SPEAKING;
        default:
            return P4_UI_DIALOG_STATE_ERROR;
    }
}

const char *P4UiModelDialogStateText(P4UiDialogState state)
{
    switch (state) {
        case P4_UI_DIALOG_STATE_CONNECTING:
            return "连接中";
        case P4_UI_DIALOG_STATE_UNKNOWN:
            return "状态同步中";
        case P4_UI_DIALOG_STATE_IDLE:
            return "等待唤醒";
        case P4_UI_DIALOG_STATE_WAKEUP:
            return "正在唤醒";
        case P4_UI_DIALOG_STATE_LISTENING:
            return "正在聆听";
        case P4_UI_DIALOG_STATE_SPEAKING:
            return "正在说话";
        case P4_UI_DIALOG_STATE_ERROR:
        default:
            return "状态异常";
    }
}

const char *P4UiModelDialogEmojiText(P4UiDialogState state)
{
    switch (state) {
        case P4_UI_DIALOG_STATE_CONNECTING:
            return "\xF0\x9F\x98\x89"; /* U+1F609 */
        case P4_UI_DIALOG_STATE_UNKNOWN:
            return "\xF0\x9F\x98\x89"; /* U+1F609 */
        case P4_UI_DIALOG_STATE_IDLE:
            return "\xF0\x9F\x98\xB4"; /* U+1F634 */
        case P4_UI_DIALOG_STATE_WAKEUP:
            return "\xF0\x9F\x98\x89"; /* U+1F609 */
        case P4_UI_DIALOG_STATE_LISTENING:
            return "\xF0\x9F\x99\x82"; /* U+1F642 */
        case P4_UI_DIALOG_STATE_SPEAKING:
            return "\xF0\x9F\x98\x83"; /* U+1F603 */
        case P4_UI_DIALOG_STATE_ERROR:
        default:
            return "\xF0\x9F\x98\x89"; /* U+1F609 fallback */
    }
}

const char *P4UiModelAgentStatusName(uint32_t status)
{
    switch (status) {
        case 0U:
            return "Idle";
        case 1U:
            return "Wakeup";
        case 2U:
            return "Listening";
        case 3U:
            return "Speaking";
        default:
            return "Unknown";
    }
}

void P4UiModelGetSnapshot(P4UiModelSnapshot *out)
{
    ConfigServiceData cfg;
    if (out == NULL) {
        return;
    }

    (void)memset(out, 0, sizeof(*out));
    OhosUartLinkUiGetSnapshot(&out->uart);
    OhosUartLinkUiGetSensorSnapshot(&out->sensor_th);
    out->dialog_state = P4UiModelMapDialogState(&out->uart);

    out->volume = 60U;
    out->brightness = 70U;
    out->interrupt_mode = out->uart.interrupt_mode;
    if (ConfigServiceIsReady() && ConfigServiceGet(&cfg) == 0U) {
        out->config_ready = 1U;
        out->volume = cfg.volume;
        out->brightness = cfg.brightness;
        out->interrupt_mode = cfg.interrupt_mode;
    }

    out->external_font_loaded = OhosLvglExternalFontIsLoaded();
    out->wifi_link_status = out->uart.wifi_link_status;
}

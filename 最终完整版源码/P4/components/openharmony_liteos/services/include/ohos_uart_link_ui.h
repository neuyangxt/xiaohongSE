#ifndef OHOS_UART_LINK_UI_H
#define OHOS_UART_LINK_UI_H

#include <stdint.h>

#include "ohos_uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t driver_init_ok;
    uint32_t rx_task_started;
    uint32_t handshake_started;
    uint32_t handshake_done;
    uint32_t link_ready;

    uint32_t tx_count;
    uint32_t query_tx_count;
    uint32_t query_tx_ok;

    uint32_t rx_frame_count;
    uint32_t rx_raw_count;
    uint32_t rx_raw_bytes;
    uint32_t rx_parse_fail_count;

    uint32_t last_rx_cmd;
    uint32_t last_system_status;
    uint32_t agent_status;
    uint32_t agent_status_rx_count;
    uint32_t agent_status_invalid_count;
    uint32_t wake_status;
    uint32_t wake_status_rx_count;
    uint32_t wake_status_invalid_count;
    uint32_t wake_tx_count;
    uint32_t wake_tx_ok;
    uint32_t wake_pending;
    uint32_t wake_pending_seq;
    uint32_t wake_pending_retries;
    uint32_t wake_pending_ack_count;
    uint32_t wake_pending_timeout_count;
    uint32_t agent_query_tx_count;
    uint32_t agent_query_tx_ok;
    uint32_t interrupt_mode;
    uint32_t interrupt_mode_rx_count;
    uint32_t interrupt_mode_invalid_count;
    uint32_t interrupt_mode_tx_count;
    uint32_t interrupt_mode_tx_ok;
    uint32_t voice_user_action_count;
    uint32_t voice_user_action_wake_count;
    uint32_t voice_user_action_query_count;
    uint32_t voice_user_action_noop_count;
    uint32_t voice_user_action_disabled_count;
    uint32_t user_interrupt_active;
    uint32_t user_interrupt_seq;
    uint32_t downlink_suppress_seq;
    uint32_t downlink_suppress_drop;
    int32_t last_write_ret;
    int32_t last_read_ret;

    uint32_t ws_url_len;
    uint32_t wifi_ssid_len;
    uint32_t wifi_pswd_len;
    uint32_t wifi_link_status;
    uint32_t down_text_count;
    uint32_t down_text_type;
    uint32_t down_text_len;
    uint32_t down_audio_count;
    uint32_t down_audio_bytes;
    uint32_t down_audio_last_len;
    uint32_t down_audio_drop_by_state;
    uint32_t down_audio_start_count;
    uint32_t down_audio_stop_count;
    uint32_t down_audio_boundary_invalid;
    uint32_t down_audio_boundary_active;
    uint32_t down_audio_stop_seen;
    uint32_t down_audio_start_busy_count;
    uint32_t down_audio_stream_open;
    uint32_t down_audio_prepared_on_start;
    uint32_t down_audio_wait_more_count;
    uint32_t down_audio_wait_more_timeout;
    uint32_t down_audio_stream_underrun;
    uint32_t down_audio_stream_min_level;
    uint32_t downlink_session_seq;
    uint32_t downlink_session_prepared_seq;
    uint32_t opus_queue_count;
    uint32_t opus_queue_drops;
    uint32_t opus_decode_ok;
    uint32_t opus_decode_fail;
    uint32_t opus_play_ok;
    uint32_t opus_play_fail;
    uint32_t opus_last_samples;
    uint32_t opus_task_started;

    uint32_t up_opus_task_started;
    uint32_t up_opus_running;
    uint32_t up_opus_start_count;
    uint32_t up_opus_stop_count;
    uint32_t up_opus_pcm_ok;
    uint32_t up_opus_pcm_fail;
    uint32_t up_opus_encode_ok;
    uint32_t up_opus_encode_fail;
    uint32_t up_opus_tx_ok;
    uint32_t up_opus_tx_fail;
    uint32_t up_opus_last_len;
    uint32_t up_opus_last_samples;
    uint32_t up_opus_last_peak;
    uint32_t up_opus_last_nonzero;
    uint32_t up_opus_last_ret;
    uint32_t up_opus_vad_start_ok;
    uint32_t up_opus_vad_start_fail;
    uint32_t up_opus_vad_end_ok;
    uint32_t up_opus_vad_end_fail;
    uint32_t audio_state;
    uint32_t audio_state_transitions;
    uint32_t audio_downlink_guard_drop;

} OhosUartLinkUiSnapshot;

typedef struct {
    uint32_t valid;
    uint32_t rx_count;
    uint32_t invalid_count;
    uint32_t last_ms;
    int32_t temp100;
    int32_t humi100;

    uint32_t sht30_online_valid;
    uint32_t sht30_online;
    uint32_t query_tx_count;
    uint32_t query_tx_ok;

    uint32_t fan_online_valid;
    uint32_t fan_online;
    uint32_t fan_switch_valid;
    uint32_t fan_switch_on;
    uint32_t fan_level_valid;
    uint32_t fan_level;
    uint32_t fan_report_count;
    uint32_t fan_last_ms;
    uint32_t fan_control_tx_count;
    uint32_t fan_control_tx_ok;
    uint32_t fan_control_ack_count;
    uint32_t fan_control_ack_seq;
    uint32_t fan_control_ack_error;
    uint32_t fan_control_ack_last_ms;

    uint32_t alarm_online_valid;
    uint32_t alarm_online;
    uint32_t alarm_switch_valid;
    uint32_t alarm_switch_on;
    uint32_t alarm_mode_valid;
    uint32_t alarm_mode;
    uint32_t alarm_gpio_output_valid;
    uint32_t alarm_gpio_output;
    uint32_t alarm_timeout_protected_valid;
    uint32_t alarm_timeout_protected;
    uint32_t alarm_report_count;
    uint32_t alarm_last_ms;
    uint32_t alarm_control_tx_count;
    uint32_t alarm_control_tx_ok;
    uint32_t alarm_control_ack_count;
    uint32_t alarm_control_ack_seq;
    uint32_t alarm_control_ack_error;
    uint32_t alarm_control_ack_last_ms;

    uint32_t light_online_valid;
    uint32_t light_online;
    uint32_t light_switch_valid;
    uint32_t light_switch_on;
    uint32_t light_rgb_valid;
    uint32_t light_r;
    uint32_t light_g;
    uint32_t light_b;
    uint32_t light_brightness;
    uint32_t light_gpio_output_valid;
    uint32_t light_gpio_output;
    uint32_t light_report_count;
    uint32_t light_last_ms;
    uint32_t light_control_tx_count;
    uint32_t light_control_tx_ok;
    uint32_t light_control_last_tx_ok;
    uint32_t light_control_tx_seq;
    uint32_t light_control_tx_ms;
    uint32_t light_control_desired_on;
    uint32_t light_control_desired_r;
    uint32_t light_control_desired_g;
    uint32_t light_control_desired_b;
    uint32_t light_control_desired_brightness;
    uint32_t light_control_ack_count;
    uint32_t light_control_ack_seq;
    uint32_t light_control_ack_error;
    uint32_t light_control_ack_last_ms;

    uint32_t scene_mode_valid;
    uint32_t scene_mode;
    uint32_t scene_cause_valid;
    uint32_t scene_cause;
    uint32_t scene_flags_valid;
    uint32_t scene_flags;
    uint32_t scene_report_count;
    uint32_t scene_last_ms;
    uint32_t scene_control_tx_count;
    uint32_t scene_control_tx_ok;
    uint32_t scene_control_last_tx_ok;
    uint32_t scene_control_tx_seq;
    uint32_t scene_control_tx_ms;
    uint32_t scene_control_desired_mode;
    uint32_t scene_control_ack_count;
    uint32_t scene_control_ack_seq;
    uint32_t scene_control_ack_error;
    uint32_t scene_control_ack_last_ms;
} OhosUartLinkSensorSnapshot;

uint32_t OhosUartLinkUiSendQuery(void);
uint32_t OhosUartLinkUiSendReportOk(void);
uint32_t OhosUartLinkUiSendWakeUp(void);
uint32_t OhosUartLinkUiQueryAgentStatus(void);
uint32_t OhosUartLinkUiSetInterruptMode(uint32_t enabled);
uint32_t OhosUartLinkUiVoiceUserAction(void);
uint32_t OhosUartLinkUiStartUplinkOpusTest(void);
uint32_t OhosUartLinkUiStopUplinkOpusTest(void);
uint32_t OhosUartLinkUiSensorQuery(void);
uint32_t OhosUartLinkUiSetFan(uint32_t on);
uint32_t OhosUartLinkUiSetAlarmMode(uint32_t mode);
uint32_t OhosUartLinkUiSetLight(uint32_t on,
                               uint32_t r,
                               uint32_t g,
                               uint32_t b,
                               uint32_t brightness);
uint32_t OhosUartLinkUiSetSceneMode(uint32_t mode);
uint32_t OhosUartLinkUiSendModuleControl(uint32_t device, uint32_t action);
void OhosUartLinkUiGetSnapshot(OhosUartLinkUiSnapshot *snap);
void OhosUartLinkUiGetSensorSnapshot(OhosUartLinkSensorSnapshot *snap);
void OhosUartLinkUiGetDownText(char *buf, uint32_t bufLen);
const char *OhosUartLinkUiCmdName(uint32_t cmd);

const char* OhosUartLinkUiGetWifiLinkIp(void);
const char* OhosUartLinkUiGetWifiScanResult(void);
uint32_t UartLinkSendWiFiStartScan(const char *tag);

#ifdef __cplusplus
}
#endif

#endif

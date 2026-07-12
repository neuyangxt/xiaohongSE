#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_SERVICE_SCHEMA_VERSION 1U
#define CONFIG_SERVICE_DEVICE_ID_HEX_MAX 33U
#define CONFIG_SERVICE_URL_MAX 160U
#define CONFIG_SERVICE_FONT_VERSION_MAX 24U
#define CONFIG_SERVICE_FONT_PATH_MAX 96U
#define CONFIG_SERVICE_SHA256_HEX_MAX 65U
#define CONFIG_SERVICE_SSID_MAX 64U
#define CONFIG_SERVICE_PSWD_MAX 64U
#define CONFIG_SERVICE_FW_VERSION_MAX 32U

#define CONFIG_SERVICE_PATH      "/data/config/config.json"
#define CONFIG_SERVICE_BAK_PATH  "/data/config/config.bak"
#define CONFIG_SERVICE_TMP_PATH  "/data/config/config.tmp"
#define CONFIG_SERVICE_DIR       "/data/config"

typedef struct {
    uint32_t schema_version;
    char device_id_hex[CONFIG_SERVICE_DEVICE_ID_HEX_MAX];
    uint32_t volume;
    uint32_t brightness;
    uint32_t interrupt_mode;
    char ota_url[CONFIG_SERVICE_URL_MAX];

    char font_version[CONFIG_SERVICE_FONT_VERSION_MAX];
    char font_path[CONFIG_SERVICE_FONT_PATH_MAX];
    uint32_t font_size;
    char font_sha256[CONFIG_SERVICE_SHA256_HEX_MAX];
    uint32_t font_valid;

    char wifi_last_ssid[CONFIG_SERVICE_SSID_MAX];
    char wifi_ssid[CONFIG_SERVICE_SSID_MAX];
    char wifi_pswd[CONFIG_SERVICE_PSWD_MAX];
    uint32_t wifi_last_success_ts;
    uint32_t wifi_priority;

    char p4_version[CONFIG_SERVICE_FW_VERSION_MAX];
    char ws63_version[CONFIG_SERVICE_FW_VERSION_MAX];

    uint32_t factory_mode;
    uint32_t first_boot;
} ConfigServiceData;

uint32_t ConfigServiceInit(void);
uint32_t ConfigServiceLoad(ConfigServiceData *out);
uint32_t ConfigServiceSave(const ConfigServiceData *cfg);
uint32_t ConfigServiceGet(ConfigServiceData *out);
uint32_t ConfigServiceSetWifiSsid(const char *ssid);
uint32_t ConfigServiceSetWifiPswd(const char *pswd);
uint32_t ConfigServiceSetOtaUrl(const char *url);
uint32_t ConfigServiceSetWs63FwVersion(const char *version);
uint32_t ConfigServiceSelfTest(void);
uint32_t ConfigServiceStartTask(void);
uint32_t ConfigServiceIsReady(void);

#ifdef __cplusplus
}
#endif

#endif

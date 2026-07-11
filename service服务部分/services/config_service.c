#include "config_service.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "device_identity_service.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "los_task.h"
#include "ohos_lfs_port.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#define CONFIG_SERVICE_READ_BUF_SIZE 4096U
#define CONFIG_SERVICE_TASK_PRIO 23
#define CONFIG_SERVICE_TASK_STACK 0x3000
#define CONFIG_SERVICE_START_DELAY_TICKS 120U
#define CONFIG_SERVICE_SAVE_POLL_TICKS   20U

static ConfigServiceData g_config_service_cache;
static volatile uint32_t g_config_service_pending_save = 0;
static volatile uint32_t g_config_service_ready = 0;
static volatile uint32_t g_config_service_init_done = 0;
static volatile uint32_t g_config_service_init_ret = 0xFFFFFFFFU;
static volatile uint32_t g_config_service_load_from_bak = 0;
static volatile uint32_t g_config_service_default_written = 0;
static UINT32 g_config_service_task_id = 0;

static void ConfigServiceCopyString(char *dst, size_t dstLen, const char *src)
{
    if (dst == NULL || dstLen == 0U) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    (void)snprintf(dst, dstLen, "%s", src);
}

static void ConfigServiceDefaults(ConfigServiceData *cfg)
{
    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->schema_version = CONFIG_SERVICE_SCHEMA_VERSION;
    ConfigServiceCopyString(cfg->device_id_hex, sizeof(cfg->device_id_hex), DeviceIdentityGetHex());
    cfg->volume = 60U;
    cfg->brightness = 70U;
    cfg->interrupt_mode = 0U;
    ConfigServiceCopyString(cfg->ota_url,
                            sizeof(cfg->ota_url),
                            "http://xiaohong-ota.yunxin360.com:8002/xiaohong/ota/");
    ConfigServiceCopyString(cfg->font_version, sizeof(cfg->font_version), "0.0.0");
    ConfigServiceCopyString(cfg->font_path, sizeof(cfg->font_path), "/data/system/font.bin");
    cfg->font_size = 0U;
    cfg->font_valid = 0U;
    ConfigServiceCopyString(cfg->p4_version, sizeof(cfg->p4_version), "0.0.0");
    cfg->factory_mode = 0U;
    cfg->first_boot = 1U;
}

static uint32_t ConfigServiceJsonGetUint(cJSON *obj, const char *name, uint32_t fallback)
{
    cJSON *item;

    if (obj == NULL || name == NULL) {
        return fallback;
    }

    item = cJSON_GetObjectItemCaseSensitive(obj, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0.0) {
        return fallback;
    }

    return (uint32_t)item->valuedouble;
}

static uint32_t ConfigServiceJsonGetBool(cJSON *obj, const char *name, uint32_t fallback)
{
    cJSON *item;

    if (obj == NULL || name == NULL) {
        return fallback;
    }

    item = cJSON_GetObjectItemCaseSensitive(obj, name);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1U : 0U;
    }

    return fallback;
}

static void ConfigServiceJsonGetString(cJSON *obj,
                                       const char *name,
                                       char *dst,
                                       size_t dstLen,
                                       const char *fallback)
{
    cJSON *item;

    if (dst == NULL || dstLen == 0U) {
        return;
    }

    item = (obj != NULL && name != NULL) ? cJSON_GetObjectItemCaseSensitive(obj, name) : NULL;
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        ConfigServiceCopyString(dst, dstLen, item->valuestring);
        return;
    }

    ConfigServiceCopyString(dst, dstLen, fallback);
}

static uint32_t ConfigServiceParseJson(const char *json, ConfigServiceData *out)
{
    ConfigServiceData cfg;
    cJSON *root;
    cJSON *font;
    cJSON *wifi;
    cJSON *profiles;
    cJSON *profile0;
    cJSON *fw;
    cJSON *flags;

    if (json == NULL || out == NULL) {
        return 1U;
    }

    ConfigServiceDefaults(&cfg);
    root = cJSON_Parse(json);
    if (!cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return 2U;
    }

    cfg.schema_version = ConfigServiceJsonGetUint(root, "schema_version", 0U);
    if (cfg.schema_version != CONFIG_SERVICE_SCHEMA_VERSION) {
        cJSON_Delete(root);
        return 3U;
    }

    ConfigServiceJsonGetString(root,
                               "device_id_hex",
                               cfg.device_id_hex,
                               sizeof(cfg.device_id_hex),
                               DeviceIdentityGetHex());
    cfg.volume = ConfigServiceJsonGetUint(root, "volume", cfg.volume);
    cfg.brightness = ConfigServiceJsonGetUint(root, "brightness", cfg.brightness);
    cfg.interrupt_mode = ConfigServiceJsonGetUint(root, "interrupt_mode", cfg.interrupt_mode);
    ConfigServiceJsonGetString(root, "ota_url", cfg.ota_url, sizeof(cfg.ota_url), cfg.ota_url);

    font = cJSON_GetObjectItemCaseSensitive(root, "font");
    ConfigServiceJsonGetString(font, "version", cfg.font_version, sizeof(cfg.font_version), cfg.font_version);
    ConfigServiceJsonGetString(font, "path", cfg.font_path, sizeof(cfg.font_path), cfg.font_path);
    cfg.font_size = ConfigServiceJsonGetUint(font, "size", cfg.font_size);
    ConfigServiceJsonGetString(font, "sha256", cfg.font_sha256, sizeof(cfg.font_sha256), cfg.font_sha256);
    cfg.font_valid = ConfigServiceJsonGetBool(font, "valid", cfg.font_valid);

    wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    ConfigServiceJsonGetString(wifi, "last_ssid", cfg.wifi_last_ssid, sizeof(cfg.wifi_last_ssid), cfg.wifi_last_ssid);
    profiles = cJSON_GetObjectItemCaseSensitive(wifi, "profiles");
    profile0 = cJSON_GetArrayItem(profiles, 0);
    ConfigServiceJsonGetString(profile0, "ssid", cfg.wifi_ssid, sizeof(cfg.wifi_ssid), cfg.wifi_ssid);
    ConfigServiceJsonGetString(profile0, "pswd", cfg.wifi_pswd, sizeof(cfg.wifi_pswd), cfg.wifi_pswd);
    cfg.wifi_last_success_ts = ConfigServiceJsonGetUint(profile0, "last_success_ts", cfg.wifi_last_success_ts);
    cfg.wifi_priority = ConfigServiceJsonGetUint(profile0, "priority", cfg.wifi_priority);

    fw = cJSON_GetObjectItemCaseSensitive(root, "fw");
    ConfigServiceJsonGetString(fw, "p4_version", cfg.p4_version, sizeof(cfg.p4_version), cfg.p4_version);
    ConfigServiceJsonGetString(fw, "ws63_version", cfg.ws63_version, sizeof(cfg.ws63_version), cfg.ws63_version);

    flags = cJSON_GetObjectItemCaseSensitive(root, "flags");
    cfg.factory_mode = ConfigServiceJsonGetBool(flags, "factory_mode", cfg.factory_mode);
    cfg.first_boot = ConfigServiceJsonGetBool(flags, "first_boot", cfg.first_boot);

    cJSON_Delete(root);
    *out = cfg;
    return 0U;
}

static cJSON *ConfigServiceCreateJson(const ConfigServiceData *cfg)
{
    cJSON *root;
    cJSON *font;
    cJSON *wifi;
    cJSON *profiles;
    cJSON *profile0;
    cJSON *fw;
    cJSON *flags;

    if (cfg == NULL) {
        return NULL;
    }

    root = cJSON_CreateObject();
    font = cJSON_CreateObject();
    wifi = cJSON_CreateObject();
    profiles = cJSON_CreateArray();
    profile0 = cJSON_CreateObject();
    fw = cJSON_CreateObject();
    flags = cJSON_CreateObject();
    if (root == NULL || font == NULL || wifi == NULL || profiles == NULL ||
        profile0 == NULL || fw == NULL || flags == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(font);
        cJSON_Delete(wifi);
        cJSON_Delete(profiles);
        cJSON_Delete(profile0);
        cJSON_Delete(fw);
        cJSON_Delete(flags);
        return NULL;
    }

    cJSON_AddNumberToObject(root, "schema_version", cfg->schema_version);
    cJSON_AddStringToObject(root, "device_id_hex", cfg->device_id_hex);
    cJSON_AddNumberToObject(root, "volume", cfg->volume);
    cJSON_AddNumberToObject(root, "brightness", cfg->brightness);
    cJSON_AddNumberToObject(root, "interrupt_mode", cfg->interrupt_mode);
    cJSON_AddStringToObject(root, "ota_url", cfg->ota_url);

    cJSON_AddStringToObject(font, "version", cfg->font_version);
    cJSON_AddStringToObject(font, "path", cfg->font_path);
    cJSON_AddNumberToObject(font, "size", cfg->font_size);
    cJSON_AddStringToObject(font, "sha256", cfg->font_sha256);
    cJSON_AddBoolToObject(font, "valid", cfg->font_valid ? 1 : 0);
    cJSON_AddItemToObject(root, "font", font);
    font = NULL;

    cJSON_AddStringToObject(wifi, "last_ssid", cfg->wifi_last_ssid);
    cJSON_AddStringToObject(profile0, "ssid", cfg->wifi_ssid);
    cJSON_AddStringToObject(profile0, "pswd", cfg->wifi_pswd);
    cJSON_AddNumberToObject(profile0, "last_success_ts", cfg->wifi_last_success_ts);
    cJSON_AddNumberToObject(profile0, "priority", cfg->wifi_priority);
    cJSON_AddItemToArray(profiles, profile0);
    profile0 = NULL;
    cJSON_AddItemToObject(wifi, "profiles", profiles);
    profiles = NULL;
    cJSON_AddItemToObject(root, "wifi", wifi);
    wifi = NULL;

    cJSON_AddStringToObject(fw, "p4_version", cfg->p4_version);
    cJSON_AddStringToObject(fw, "ws63_version", cfg->ws63_version);
    cJSON_AddItemToObject(root, "fw", fw);
    fw = NULL;

    cJSON_AddBoolToObject(flags, "factory_mode", cfg->factory_mode ? 1 : 0);
    cJSON_AddBoolToObject(flags, "first_boot", cfg->first_boot ? 1 : 0);
    cJSON_AddItemToObject(root, "flags", flags);
    flags = NULL;

    return root;
}

static uint32_t ConfigServiceReadFile(const char *path, ConfigServiceData *out)
{
    char *buf;
    size_t readLen = 0;
    uint32_t ret;

    if (path == NULL || out == NULL) {
        return 1U;
    }

    buf = (char *)heap_caps_malloc(CONFIG_SERVICE_READ_BUF_SIZE, MALLOC_CAP_8BIT);
    if (buf == NULL) {
        return 2U;
    }

    ret = OhosLfsPortReadTextWithLen(path, buf, CONFIG_SERVICE_READ_BUF_SIZE, &readLen);
    if (ret != 0U) {
        heap_caps_free(buf);
        return 10U + ret;
    }

    ret = ConfigServiceParseJson(buf, out);
    heap_caps_free(buf);
    if (ret != 0U) {
        return 20U + ret;
    }

    esp_rom_printf("[P4-CONFIG] read ok path=%s bytes=%u volume=%u brightness=%u device=%s\n",
                   path,
                   (unsigned)readLen,
                   out->volume,
                   out->brightness,
                   out->device_id_hex);
    return 0U;
}

uint32_t ConfigServiceSave(const ConfigServiceData *cfg)
{
    cJSON *root;
    char *json;
    ConfigServiceData verifyCfg;
    uint32_t ret;

    if (cfg == NULL) {
        return 1U;
    }

    ret = OhosLfsPortEnsureDir(CONFIG_SERVICE_DIR);
    if (ret != 0U) {
        return 10U + ret;
    }

    root = ConfigServiceCreateJson(cfg);
    if (root == NULL) {
        return 2U;
    }

    json = cJSON_Print(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return 3U;
    }

    ret = OhosLfsPortWriteText(CONFIG_SERVICE_TMP_PATH, json);
    cJSON_free(json);
    if (ret != 0U) {
        return 20U + ret;
    }

    ret = ConfigServiceReadFile(CONFIG_SERVICE_TMP_PATH, &verifyCfg);
    if (ret != 0U) {
        (void)OhosLfsPortDelete(CONFIG_SERVICE_TMP_PATH);
        return 30U + ret;
    }

    if (OhosLfsPortFileExists(CONFIG_SERVICE_BAK_PATH)) {
        (void)remove(CONFIG_SERVICE_BAK_PATH);
    }

    if (OhosLfsPortFileExists(CONFIG_SERVICE_PATH)) {
        if (rename(CONFIG_SERVICE_PATH, CONFIG_SERVICE_BAK_PATH) != 0) {
            esp_rom_printf("[P4-CONFIG] rename old->bak failed errno=%d\n", errno);
            (void)OhosLfsPortDelete(CONFIG_SERVICE_TMP_PATH);
            return 4U;
        }
    }

    if (rename(CONFIG_SERVICE_TMP_PATH, CONFIG_SERVICE_PATH) != 0) {
        esp_rom_printf("[P4-CONFIG] rename tmp->config failed errno=%d\n", errno);
        return 5U;
    }

    g_config_service_cache = verifyCfg;
    g_config_service_ready = 1U;
    esp_rom_printf("[P4-CONFIG] save ok path=%s schema=%u volume=%u brightness=%u firstBoot=%u\n",
                   CONFIG_SERVICE_PATH,
                   verifyCfg.schema_version,
                   verifyCfg.volume,
                   verifyCfg.brightness,
                   verifyCfg.first_boot);
    return 0U;
}

uint32_t ConfigServiceLoad(ConfigServiceData *out)
{
    ConfigServiceData cfg;
    uint32_t ret;

    if (out == NULL) {
        return 1U;
    }

    ret = OhosLfsPortEnsureDir(CONFIG_SERVICE_DIR);
    if (ret != 0U) {
        ConfigServiceDefaults(out);
        return 10U + ret;
    }

    ret = ConfigServiceReadFile(CONFIG_SERVICE_PATH, &cfg);
    if (ret == 0U) {
        *out = cfg;
        return 0U;
    }

    esp_rom_printf("[P4-CONFIG] primary load failed ret=%u, try bak\n", ret);
    ret = ConfigServiceReadFile(CONFIG_SERVICE_BAK_PATH, &cfg);
    if (ret == 0U) {
        g_config_service_load_from_bak++;
        *out = cfg;
        (void)ConfigServiceSave(&cfg);
        return 0U;
    }

    esp_rom_printf("[P4-CONFIG] bak load failed ret=%u, use defaults\n", ret);
    ConfigServiceDefaults(&cfg);
    *out = cfg;
    return 100U;
}

uint32_t ConfigServiceInit(void)
{
    ConfigServiceData cfg;
    uint32_t loadRet;
    uint32_t saveRet = 0U;

    if (g_config_service_init_done) {
        return g_config_service_init_ret;
    }

    g_config_service_init_done = 1U;
    (void)OhosLfsPortInit();
    loadRet = ConfigServiceLoad(&cfg);
    if (loadRet == 100U) {
        saveRet = ConfigServiceSave(&cfg);
        if (saveRet == 0U) {
            g_config_service_default_written++;
        }
    } else if (loadRet == 0U) {
        g_config_service_cache = cfg;
        g_config_service_ready = 1U;
        if (!OhosLfsPortFileExists(CONFIG_SERVICE_BAK_PATH)) {
            saveRet = ConfigServiceSave(&cfg);
            esp_rom_printf("[P4-CONFIG] bak missing, refresh atomic save ret=%u\n", saveRet);
        }
    }

    g_config_service_init_ret = (loadRet == 0U || (loadRet == 100U && saveRet == 0U)) ? 0U : loadRet;
    esp_rom_printf("[P4-CONFIG] init done ret=%u loadRet=%u saveRet=%u ready=%u defaultWritten=%u bakLoad=%u path=%s\n",
                   g_config_service_init_ret,
                   loadRet,
                   saveRet,
                   g_config_service_ready,
                   g_config_service_default_written,
                   g_config_service_load_from_bak,
                   CONFIG_SERVICE_PATH);
    return g_config_service_init_ret;
}

uint32_t ConfigServiceGet(ConfigServiceData *out)
{
    if (out == NULL) {
        return 1U;
    }

    if (!g_config_service_init_done) {
        (void)ConfigServiceInit();
    }

    if (!g_config_service_ready) {
        ConfigServiceDefaults(out);
        return 2U;
    }

    *out = g_config_service_cache;
    return 0U;
}

static uint32_t ConfigServiceEnsureReadyForUpdate(void)
{
    if (!g_config_service_init_done) {
        (void)ConfigServiceInit();
    }

    if (!g_config_service_ready) {
        ConfigServiceDefaults(&g_config_service_cache);
        g_config_service_ready = 1U;
    }

    return 0U;
}

static void ConfigServiceRequestDeferredSave(void)
{
    g_config_service_pending_save = 1U;
}

static void ConfigServiceFlushPendingSave(void)
{
    ConfigServiceData snap;
    uint32_t ret;

    if (!g_config_service_pending_save) {
        return;
    }

    g_config_service_pending_save = 0U;
    snap = g_config_service_cache;
    ret = ConfigServiceSave(&snap);
    if (ret != 0U) {
        g_config_service_pending_save = 1U;
        esp_rom_printf("[P4-CONFIG] deferred save failed ret=%u, will retry\n", ret);
    }
}

uint32_t ConfigServiceSetWifiSsid(const char *ssid)
{
    uint32_t ret;

    if (ssid == NULL) {
        return 1U;
    }

    ret = ConfigServiceEnsureReadyForUpdate();
    if (ret != 0U) {
        return ret;
    }

    ConfigServiceCopyString(g_config_service_cache.wifi_ssid,
                            sizeof(g_config_service_cache.wifi_ssid),
                            ssid);
    ConfigServiceRequestDeferredSave();
    return 0U;
}

uint32_t ConfigServiceSetWifiPswd(const char *pswd)
{
    uint32_t ret;

    if (pswd == NULL) {
        return 1U;
    }

    ret = ConfigServiceEnsureReadyForUpdate();
    if (ret != 0U) {
        return ret;
    }

    ConfigServiceCopyString(g_config_service_cache.wifi_pswd,
                            sizeof(g_config_service_cache.wifi_pswd),
                            pswd);
    ConfigServiceRequestDeferredSave();
    return 0U;
}

uint32_t ConfigServiceSetOtaUrl(const char *url)
{
    uint32_t ret;

    if (url == NULL) {
        return 1U;
    }

    ret = ConfigServiceEnsureReadyForUpdate();
    if (ret != 0U) {
        return ret;
    }

    ConfigServiceCopyString(g_config_service_cache.ota_url,
                            sizeof(g_config_service_cache.ota_url),
                            url);
    ConfigServiceRequestDeferredSave();
    return 0U;
}

uint32_t ConfigServiceSetWs63FwVersion(const char *version)
{
    uint32_t ret;

    if (version == NULL) {
        return 1U;
    }

    ret = ConfigServiceEnsureReadyForUpdate();
    if (ret != 0U) {
        return ret;
    }

    ConfigServiceCopyString(g_config_service_cache.ws63_version,
                            sizeof(g_config_service_cache.ws63_version),
                            version);
    ConfigServiceRequestDeferredSave();
    return 0U;
}
uint32_t ConfigServiceSelfTest(void)
{
    ConfigServiceData cfg;
    ConfigServiceData verify;
    uint32_t ret;

    ret = ConfigServiceInit();
    if (ret != 0U) {
        esp_rom_printf("[P4-CONFIG] selftest init failed ret=%u\n", ret);
        return ret;
    }

    ret = ConfigServiceGet(&cfg);
    if (ret != 0U) {
        esp_rom_printf("[P4-CONFIG] selftest get failed ret=%u\n", ret);
        return 10U + ret;
    }

    ret = ConfigServiceLoad(&verify);
    if (ret != 0U) {
        esp_rom_printf("[P4-CONFIG] selftest reload failed ret=%u\n", ret);
        return 20U + ret;
    }

    if (verify.schema_version != CONFIG_SERVICE_SCHEMA_VERSION ||
        verify.volume > 100U ||
        verify.brightness > 100U) {
        esp_rom_printf("[P4-CONFIG] selftest invalid values schema=%u volume=%u brightness=%u\n",
                       verify.schema_version,
                       verify.volume,
                       verify.brightness);
        return 30U;
    }

    esp_rom_printf("[P4-CONFIG] selftest ok device=%s volume=%u brightness=%u ota=%s\n",
                   verify.device_id_hex,
                   verify.volume,
                   verify.brightness,
                   verify.ota_url);
    return 0U;
}

uint32_t ConfigServiceIsReady(void)
{
    return g_config_service_ready;
}

static VOID *ConfigServiceTask(UINT32 arg)
{
    (void)arg;
    LOS_TaskDelay(CONFIG_SERVICE_START_DELAY_TICKS);
    (void)ConfigServiceSelfTest();
    for (;;) {
        ConfigServiceFlushPendingSave();
        (void)LOS_TaskDelay(CONFIG_SERVICE_SAVE_POLL_TICKS);
    }
    return NULL;
}

uint32_t ConfigServiceStartTask(void)
{
    TSK_INIT_PARAM_S task = {0};
    uint32_t ret;

    if (g_config_service_task_id != 0U) {
        return LOS_OK;
    }

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)ConfigServiceTask;
    task.uwStackSize = CONFIG_SERVICE_TASK_STACK;
    task.usTaskPrio = CONFIG_SERVICE_TASK_PRIO;
    task.pcName = "p4_config";

    ret = LOS_TaskCreate(&g_config_service_task_id, &task);
    esp_rom_printf("[P4-CONFIG] task create ret=%u taskId=%u prio=%u stack=0x%x\n",
                   ret,
                   g_config_service_task_id,
                   CONFIG_SERVICE_TASK_PRIO,
                   CONFIG_SERVICE_TASK_STACK);
    return ret;
}

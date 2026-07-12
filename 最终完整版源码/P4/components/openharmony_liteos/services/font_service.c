#include "font_service.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "config_service.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "los_task.h"
#include "mbedtls/sha256.h"
#include "ohos_lfs_port.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

#define FONT_SERVICE_TASK_PRIO 27
#define FONT_SERVICE_TASK_STACK 0x3000
#define FONT_SERVICE_START_DELAY_TICKS 180U
#define FONT_SERVICE_READ_BUF_SIZE 1024U
#define FONT_SERVICE_MIN_BIN_SIZE 64U

typedef struct {
    uint32_t version;
    uint16_t tables_count;
    uint16_t font_size;
    uint16_t ascent;
    int16_t descent;
    uint16_t typo_ascent;
    int16_t typo_descent;
    uint16_t typo_line_gap;
    int16_t min_y;
    int16_t max_y;
    uint16_t default_advance_width;
    uint16_t kerning_scale;
    uint8_t index_to_loc_format;
    uint8_t glyph_id_format;
    uint8_t advance_width_format;
    uint8_t bits_per_pixel;
    uint8_t xy_bits;
    uint8_t wh_bits;
    uint8_t advance_width_bits;
    uint8_t compression_id;
    uint8_t subpixels_mode;
    uint8_t padding;
    int16_t underline_position;
    uint16_t underline_thickness;
} FontServiceLvglBinHeader;

extern const uint8_t p4_demo_font_start[] asm("_binary_test_font_1_fnt_start");
extern const uint8_t p4_demo_font_end[] asm("_binary_test_font_1_fnt_end");

static volatile uint32_t g_font_service_ready = 0U;
static volatile uint32_t g_font_service_init_done = 0U;
static volatile uint32_t g_font_service_init_ret = 0xffffffffU;
static UINT32 g_font_service_task_id = 0U;
static FontServiceMeta g_font_service_meta;

static void FontServiceCopyString(char *dst, size_t dstLen, const char *src)
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

static void FontServiceHex(const uint8_t *in, size_t len, char *out, size_t outLen)
{
    static const char hex[] = "0123456789abcdef";
    size_t need = (len * 2U) + 1U;

    if (out == NULL || outLen == 0U) {
        return;
    }
    out[0] = '\0';
    if (in == NULL || outLen < need) {
        return;
    }

    for (size_t i = 0; i < len; ++i) {
        out[i * 2U] = hex[(in[i] >> 4) & 0x0fU];
        out[(i * 2U) + 1U] = hex[in[i] & 0x0fU];
    }
    out[len * 2U] = '\0';
}

static uint32_t FontServiceSha256File(const char *path, char outHex[FONT_SERVICE_SHA256_MAX])
{
    uint8_t buf[256];
    uint8_t digest[32];
    mbedtls_sha256_context ctx;
    FILE *fp;

    if (path == NULL || outHex == NULL) {
        return 1U;
    }
    outHex[0] = '\0';

    fp = fopen(path, "rb");
    if (fp == NULL) {
        esp_rom_printf("[P4-FONT] sha open failed path=%s errno=%d\n", path, errno);
        return 2U;
    }

    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts(&ctx, 0) != 0) {
        fclose(fp);
        mbedtls_sha256_free(&ctx);
        return 3U;
    }

    while (1) {
        size_t n = fread(buf, 1, sizeof(buf), fp);
        if (n > 0U && mbedtls_sha256_update(&ctx, buf, n) != 0) {
            fclose(fp);
            mbedtls_sha256_free(&ctx);
            return 4U;
        }
        if (n < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                mbedtls_sha256_free(&ctx);
                return 5U;
            }
            break;
        }
    }

    fclose(fp);
    if (mbedtls_sha256_finish(&ctx, digest) != 0) {
        mbedtls_sha256_free(&ctx);
        return 6U;
    }
    mbedtls_sha256_free(&ctx);

    FontServiceHex(digest, sizeof(digest), outHex, FONT_SERVICE_SHA256_MAX);
    return 0U;
}

static uint32_t FontServiceLooksLikeLvglBin(const char *path)
{
    uint8_t chunk[8] = {0};
    FontServiceLvglBinHeader header;
    FILE *fp;
    size_t n;
    uint32_t chunkLen;

    if (path == NULL) {
        return 1U;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return 2U;
    }
    n = fread(chunk, 1, sizeof(chunk), fp);
    if (n < sizeof(chunk)) {
        fclose(fp);
        return 3U;
    }

    chunkLen = (uint32_t)chunk[0] |
               ((uint32_t)chunk[1] << 8) |
               ((uint32_t)chunk[2] << 16) |
               ((uint32_t)chunk[3] << 24);
    if (chunkLen < sizeof(FontServiceLvglBinHeader) || memcmp(&chunk[4], "head", 4U) != 0) {
        fclose(fp);
        return 4U;
    }

    memset(&header, 0, sizeof(header));
    n = fread(&header, 1, sizeof(header), fp);
    fclose(fp);
    if (n < sizeof(header)) {
        return 5U;
    }

    if (header.tables_count == 0U || header.font_size == 0U || header.bits_per_pixel == 0U) {
        esp_rom_printf("[P4-FONT] invalid LVGL bin header tables=%u size=%u bpp=%u\n",
                       header.tables_count,
                       header.font_size,
                       header.bits_per_pixel);
        return 6U;
    }

    esp_rom_printf("[P4-FONT] LVGL bin header ok size=%u bpp=%u compression=%u\n",
                   header.font_size,
                   header.bits_per_pixel,
                   header.compression_id);

    return 0U;
}

static uint32_t FontServiceSaveMeta(const FontServiceMeta *meta)
{
    cJSON *root;
    char *json;
    uint32_t ret;

    if (meta == NULL) {
        return 1U;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return 2U;
    }

    cJSON_AddStringToObject(root, "version", meta->version);
    cJSON_AddNumberToObject(root, "size", (double)meta->size);
    cJSON_AddStringToObject(root, "sha256", meta->sha256);
    cJSON_AddStringToObject(root, "format", meta->format);
    cJSON_AddBoolToObject(root, "valid", meta->valid ? 1 : 0);
    cJSON_AddNumberToObject(root, "updated_at", meta->updated_at);

    json = cJSON_Print(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return 3U;
    }

    ret = OhosLfsPortWriteText(FONT_SERVICE_META_PATH, json);
    cJSON_free(json);
    return ret;
}

uint32_t FontServiceLoadMeta(FontServiceMeta *out)
{
    char *buf;
    cJSON *root;
    cJSON *item;
    size_t readLen = 0U;
    uint32_t ret;
    FontServiceMeta meta;

    if (out == NULL) {
        return 1U;
    }

    memset(&meta, 0, sizeof(meta));
    buf = (char *)heap_caps_malloc(FONT_SERVICE_READ_BUF_SIZE, MALLOC_CAP_8BIT);
    if (buf == NULL) {
        return 2U;
    }

    ret = OhosLfsPortReadTextWithLen(FONT_SERVICE_META_PATH, buf, FONT_SERVICE_READ_BUF_SIZE, &readLen);
    if (ret != 0U) {
        heap_caps_free(buf);
        return 10U + ret;
    }

    root = cJSON_Parse(buf);
    heap_caps_free(buf);
    if (!cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return 20U;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "version");
    FontServiceCopyString(meta.version,
                          sizeof(meta.version),
                          cJSON_IsString(item) ? item->valuestring : "0.0.0");

    item = cJSON_GetObjectItemCaseSensitive(root, "size");
    meta.size = cJSON_IsNumber(item) && item->valuedouble >= 0.0 ? (size_t)item->valuedouble : 0U;

    item = cJSON_GetObjectItemCaseSensitive(root, "sha256");
    FontServiceCopyString(meta.sha256,
                          sizeof(meta.sha256),
                          cJSON_IsString(item) ? item->valuestring : "");

    item = cJSON_GetObjectItemCaseSensitive(root, "format");
    FontServiceCopyString(meta.format,
                          sizeof(meta.format),
                          cJSON_IsString(item) ? item->valuestring : "lvgl-bin");

    item = cJSON_GetObjectItemCaseSensitive(root, "valid");
    meta.valid = cJSON_IsBool(item) && cJSON_IsTrue(item) ? 1U : 0U;

    item = cJSON_GetObjectItemCaseSensitive(root, "updated_at");
    meta.updated_at = cJSON_IsNumber(item) && item->valuedouble >= 0.0 ? (uint32_t)item->valuedouble : 0U;

    cJSON_Delete(root);
    *out = meta;
    return 0U;
}

static uint32_t FontServiceSyncConfig(const FontServiceMeta *meta)
{
    ConfigServiceData cfg;
    uint32_t ret;

    if (meta == NULL || !meta->valid) {
        return 1U;
    }

    ret = ConfigServiceGet(&cfg);
    if (ret != 0U) {
        (void)ConfigServiceInit();
        ret = ConfigServiceGet(&cfg);
    }
    if (ret != 0U) {
        esp_rom_printf("[P4-FONT] config get failed ret=%u, skip font sync\n", ret);
        return 10U + ret;
    }

    FontServiceCopyString(cfg.font_version, sizeof(cfg.font_version), meta->version);
    FontServiceCopyString(cfg.font_path, sizeof(cfg.font_path), FONT_SERVICE_PATH);
    cfg.font_size = (uint32_t)meta->size;
    FontServiceCopyString(cfg.font_sha256, sizeof(cfg.font_sha256), meta->sha256);
    cfg.font_valid = 1U;

    ret = ConfigServiceSave(&cfg);
    esp_rom_printf("[P4-FONT] config sync ret=%u version=%s size=%u\n",
                   ret,
                   meta->version,
                   (unsigned)meta->size);
    return ret;
}

uint32_t FontServiceProbe(FontServiceMeta *out)
{
    FontServiceMeta meta;
    size_t size = 0U;
    uint32_t ret;

    memset(&meta, 0, sizeof(meta));
    FontServiceCopyString(meta.version, sizeof(meta.version), "0.0.0");
    FontServiceCopyString(meta.format, sizeof(meta.format), "lvgl-bin");

    ret = OhosLfsPortGetFileSize(FONT_SERVICE_PATH, &size);
    if (ret != 0U) {
        if (out != NULL) {
            *out = meta;
        }
        return 10U + ret;
    }

    meta.size = size;
    if (size < FONT_SERVICE_MIN_BIN_SIZE) {
        if (out != NULL) {
            *out = meta;
        }
        return 20U;
    }

    ret = FontServiceLooksLikeLvglBin(FONT_SERVICE_PATH);
    if (ret != 0U) {
        if (out != NULL) {
            *out = meta;
        }
        return 30U + ret;
    }

    ret = FontServiceSha256File(FONT_SERVICE_PATH, meta.sha256);
    if (ret != 0U) {
        if (out != NULL) {
            *out = meta;
        }
        return 40U + ret;
    }

    ret = FontServiceLoadMeta(&meta);
    if (ret != 0U) {
        FontServiceCopyString(meta.version, sizeof(meta.version), "dev-test-1");
        FontServiceCopyString(meta.format, sizeof(meta.format), "lvgl-bin");
        meta.size = size;
        meta.valid = 1U;
        (void)FontServiceSha256File(FONT_SERVICE_PATH, meta.sha256);
        (void)FontServiceSaveMeta(&meta);
    } else {
        char actualSha[FONT_SERVICE_SHA256_MAX] = {0};
        (void)FontServiceSha256File(FONT_SERVICE_PATH, actualSha);
        meta.size = size;
        FontServiceCopyString(meta.sha256, sizeof(meta.sha256), actualSha);
        FontServiceCopyString(meta.format, sizeof(meta.format), "lvgl-bin");
        meta.valid = 1U;
    }

    g_font_service_meta = meta;
    g_font_service_ready = 1U;
    if (out != NULL) {
        *out = meta;
    }

    esp_rom_printf("[P4-FONT] probe ok path=%s version=%s size=%u sha=%s\n",
                   FONT_SERVICE_PATH,
                   meta.version,
                   (unsigned)meta.size,
                   meta.sha256);
    return 0U;
}

uint32_t FontServiceInstallFontBuffer(const void *data, size_t len, const char *version, uint32_t updated_at)
{
    FontServiceMeta meta;
    uint32_t ret;

    if (data == NULL || len < FONT_SERVICE_MIN_BIN_SIZE) {
        return 1U;
    }

    ret = OhosLfsPortEnsureDir(FONT_SERVICE_DIR);
    if (ret != 0U) {
        return 10U + ret;
    }

    ret = OhosLfsPortWriteBinary(FONT_SERVICE_TMP_PATH, data, len);
    if (ret != 0U) {
        return 20U + ret;
    }

    ret = FontServiceLooksLikeLvglBin(FONT_SERVICE_TMP_PATH);
    if (ret != 0U) {
        (void)OhosLfsPortDelete(FONT_SERVICE_TMP_PATH);
        return 30U + ret;
    }

    if (OhosLfsPortFileExists(FONT_SERVICE_PATH)) {
        (void)remove(FONT_SERVICE_PATH);
    }

    if (rename(FONT_SERVICE_TMP_PATH, FONT_SERVICE_PATH) != 0) {
        esp_rom_printf("[P4-FONT] rename tmp->font failed errno=%d\n", errno);
        return 40U;
    }

    memset(&meta, 0, sizeof(meta));
    FontServiceCopyString(meta.version, sizeof(meta.version), version != NULL ? version : "0.0.0");
    meta.size = len;
    FontServiceCopyString(meta.format, sizeof(meta.format), "lvgl-bin");
    meta.valid = 1U;
    meta.updated_at = updated_at;
    ret = FontServiceSha256File(FONT_SERVICE_PATH, meta.sha256);
    if (ret != 0U) {
        return 50U + ret;
    }

    ret = FontServiceSaveMeta(&meta);
    if (ret != 0U) {
        return 60U + ret;
    }

    g_font_service_meta = meta;
    g_font_service_ready = 1U;
    (void)FontServiceSyncConfig(&meta);

    esp_rom_printf("[P4-FONT] install ok path=%s version=%s size=%u sha=%s\n",
                   FONT_SERVICE_PATH,
                   meta.version,
                   (unsigned)meta.size,
                   meta.sha256);
    return 0U;
}

uint32_t FontServiceEnsureDemoFont(void)
{
    const size_t demoLen = (size_t)(p4_demo_font_end - p4_demo_font_start);
    FontServiceMeta meta;
    uint32_t ret;

    ret = FontServiceProbe(&meta);
    if (ret == 0U && meta.valid) {
        return 0U;
    }

    esp_rom_printf("[P4-FONT] no valid external font, install embedded demo font ret=%u len=%u\n",
                   ret,
                   (unsigned)demoLen);
    return FontServiceInstallFontBuffer(p4_demo_font_start, demoLen, "dev-test-1", 0U);
}

uint32_t FontServiceInit(void)
{
    uint32_t ret;

    if (g_font_service_init_done) {
        return g_font_service_init_ret;
    }

    g_font_service_init_done = 1U;
    (void)OhosLfsPortInit();
    ret = FontServiceEnsureDemoFont();
    g_font_service_init_ret = ret;
    esp_rom_printf("[P4-FONT] init done ret=%u ready=%u path=%s meta=%s\n",
                   ret,
                   g_font_service_ready,
                   FONT_SERVICE_PATH,
                   FONT_SERVICE_META_PATH);
    return ret;
}

uint32_t FontServiceSelfTest(void)
{
    FontServiceMeta meta;
    uint32_t ret;

    ret = FontServiceInit();
    if (ret != 0U) {
        esp_rom_printf("[P4-FONT] selftest init failed ret=%u\n", ret);
        return ret;
    }

    ret = FontServiceProbe(&meta);
    if (ret != 0U || !meta.valid) {
        esp_rom_printf("[P4-FONT] selftest probe failed ret=%u valid=%u\n", ret, meta.valid);
        return 10U + ret;
    }

    esp_rom_printf("[P4-FONT] selftest ok version=%s size=%u sha=%s lv=%s\n",
                   meta.version,
                   (unsigned)meta.size,
                   meta.sha256,
                   FONT_SERVICE_LV_PATH);
    return 0U;
}

uint32_t FontServiceIsReady(void)
{
    return g_font_service_ready;
}

static VOID *FontServiceTask(UINT32 arg)
{
    (void)arg;
    LOS_TaskDelay(FONT_SERVICE_START_DELAY_TICKS);
    (void)FontServiceSelfTest();
    return NULL;
}

uint32_t FontServiceStartTask(void)
{
    TSK_INIT_PARAM_S task = {0};
    uint32_t ret;

    if (g_font_service_task_id != 0U) {
        return LOS_OK;
    }

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)FontServiceTask;
    task.uwStackSize = FONT_SERVICE_TASK_STACK;
    task.usTaskPrio = FONT_SERVICE_TASK_PRIO;
    task.pcName = "p4_font";

    ret = LOS_TaskCreate(&g_font_service_task_id, &task);
    esp_rom_printf("[P4-FONT] task create ret=%u taskId=%u prio=%u stack=0x%x\n",
                   ret,
                   g_font_service_task_id,
                   FONT_SERVICE_TASK_PRIO,
                   FONT_SERVICE_TASK_STACK);
    return ret;
}

#include "lvgl_lfs_font.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "esp_rom_sys.h"
#include "font_service.h"
#include "lv_font_stream_bin.h"
#include "ohos_lfs_port.h"

#define OHOS_LVGL_LFS_FONT_LETTER 'L'
#define OHOS_LVGL_LFS_FONT_PREFIX OHOS_LFS_BASE_PATH
#define OHOS_LVGL_LFS_PATH_MAX 192U
#define OHOS_LVGL_BINFONT_FALLBACK_MAX_BYTES (64U * 1024U)
#ifndef OHOS_LVGL_STREAM_FONT_ENABLE
#define OHOS_LVGL_STREAM_FONT_ENABLE 0
#endif

static lv_fs_drv_t g_ohos_lvgl_lfs_drv;
static uint32_t g_ohos_lvgl_lfs_registered;
static lv_font_t *g_ohos_external_font;
static uint32_t g_ohos_external_font_stream;

static const char *OhosLvglLfsSkipSlash(const char *path)
{
    if (path == NULL) {
        return "";
    }
    while (*path == '/') {
        path++;
    }
    return path;
}

static uint32_t OhosLvglLfsMapPath(const char *path, char *out, size_t outLen)
{
    const char *rel = OhosLvglLfsSkipSlash(path);
    int n;

    if (out == NULL || outLen == 0U) {
        return 1U;
    }

    if (rel[0] == '\0') {
        n = snprintf(out, outLen, "%s", OHOS_LVGL_LFS_FONT_PREFIX);
    } else {
        n = snprintf(out, outLen, "%s/%s", OHOS_LVGL_LFS_FONT_PREFIX, rel);
    }

    if (n < 0 || (size_t)n >= outLen) {
        out[0] = '\0';
        return 2U;
    }
    return 0U;
}

static bool OhosLvglLfsReady(lv_fs_drv_t *drv)
{
    (void)drv;
    return OhosLfsPortInit() == 0U;
}

static void *OhosLvglLfsOpen(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    char full[OHOS_LVGL_LFS_PATH_MAX];
    const char *flags = NULL;

    (void)drv;
    if ((mode & LV_FS_MODE_WR) != 0U) {
        esp_rom_printf("[P4-LVFONT] deny LVGL write path=%s\n", path != NULL ? path : "(null)");
        return NULL;
    }

    if ((mode & LV_FS_MODE_RD) != 0U) {
        flags = "rb";
    }
    if (flags == NULL || OhosLvglLfsMapPath(path, full, sizeof(full)) != 0U) {
        return NULL;
    }

    FILE *fp = fopen(full, flags);
    if (fp == NULL) {
        esp_rom_printf("[P4-LVFONT] open failed lv=%s full=%s errno=%d\n",
                       path != NULL ? path : "(null)",
                       full,
                       errno);
    }
    return fp;
}

static lv_fs_res_t OhosLvglLfsClose(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    if (file_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    return fclose((FILE *)file_p) == 0 ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t OhosLvglLfsRead(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    size_t n;

    (void)drv;
    if (br != NULL) {
        *br = 0U;
    }
    if (file_p == NULL || buf == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    n = fread(buf, 1, btr, (FILE *)file_p);
    if (br != NULL) {
        *br = (uint32_t)n;
    }
    if (n < btr && ferror((FILE *)file_p)) {
        return LV_FS_RES_UNKNOWN;
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t OhosLvglLfsSeek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    int stdWhence;

    (void)drv;
    if (file_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    switch (whence) {
        case LV_FS_SEEK_SET:
            stdWhence = SEEK_SET;
            break;
        case LV_FS_SEEK_CUR:
            stdWhence = SEEK_CUR;
            break;
        case LV_FS_SEEK_END:
            stdWhence = SEEK_END;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }

    return fseek((FILE *)file_p, (long)pos, stdWhence) == 0 ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t OhosLvglLfsTell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    long pos;

    (void)drv;
    if (file_p == NULL || pos_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    pos = ftell((FILE *)file_p);
    if (pos < 0) {
        return LV_FS_RES_UNKNOWN;
    }
    *pos_p = (uint32_t)pos;
    return LV_FS_RES_OK;
}

uint32_t OhosLvglLfsFontRegister(void)
{
    if (g_ohos_lvgl_lfs_registered) {
        return 0U;
    }

    lv_fs_drv_init(&g_ohos_lvgl_lfs_drv);
    g_ohos_lvgl_lfs_drv.letter = OHOS_LVGL_LFS_FONT_LETTER;
    g_ohos_lvgl_lfs_drv.cache_size = 512U;
    g_ohos_lvgl_lfs_drv.ready_cb = OhosLvglLfsReady;
    g_ohos_lvgl_lfs_drv.open_cb = OhosLvglLfsOpen;
    g_ohos_lvgl_lfs_drv.close_cb = OhosLvglLfsClose;
    g_ohos_lvgl_lfs_drv.read_cb = OhosLvglLfsRead;
    g_ohos_lvgl_lfs_drv.seek_cb = OhosLvglLfsSeek;
    g_ohos_lvgl_lfs_drv.tell_cb = OhosLvglLfsTell;
    lv_fs_drv_register(&g_ohos_lvgl_lfs_drv);

    g_ohos_lvgl_lfs_registered = 1U;
    esp_rom_printf("[P4-LVFONT] registered LVGL FS %c: -> %s\n",
                   OHOS_LVGL_LFS_FONT_LETTER,
                   OHOS_LVGL_LFS_FONT_PREFIX);
    return 0U;
}

uint32_t OhosLvglExternalFontLoad(void)
{
    FontServiceMeta meta;
    size_t fileSize = 0U;
    uint32_t ret;

    if (g_ohos_external_font != NULL) {
        return 0U;
    }

    ret = OhosLvglLfsFontRegister();
    if (ret != 0U) {
        return 10U + ret;
    }

    if (!FontServiceIsReady()) {
        esp_rom_printf("[P4-LVFONT] font service not ready, fallback default font for now\n");
        return 20U;
    }

    ret = FontServiceLoadMeta(&meta);
    if (ret != 0U || !meta.valid) {
        esp_rom_printf("[P4-LVFONT] meta invalid ret=%u valid=%u, fallback default font\n",
                       ret,
                       meta.valid);
        return 30U + ret;
    }

    ret = OhosLfsPortGetFileSize(FONT_SERVICE_PATH, &fileSize);
    if (ret != 0U || fileSize != meta.size) {
        esp_rom_printf("[P4-LVFONT] size check failed ret=%u file=%u meta=%u, fallback default font\n",
                       ret,
                       (unsigned)fileSize,
                       (unsigned)meta.size);
        return 40U + ret;
    }

    if (fileSize > OHOS_LVGL_BINFONT_FALLBACK_MAX_BYTES) {
        esp_rom_printf("[P4-LVFONT] external font too large for safe binfont load size=%u max=%u\n",
                       (unsigned)fileSize,
                       (unsigned)OHOS_LVGL_BINFONT_FALLBACK_MAX_BYTES);
        return 50U;
    }

#if OHOS_LVGL_STREAM_FONT_ENABLE
    g_ohos_external_font = lv_font_stream_bin_create(FONT_SERVICE_LV_PATH);
    if (g_ohos_external_font != NULL) {
        g_ohos_external_font_stream = 1U;
    } else
#endif
    {
        esp_rom_printf("[P4-LVFONT] use official binfont loader path=%s size=%u\n",
                       FONT_SERVICE_LV_PATH,
                       (unsigned)fileSize);
        g_ohos_external_font = lv_binfont_create(FONT_SERVICE_LV_PATH);
        if (g_ohos_external_font == NULL) {
            esp_rom_printf("[P4-LVFONT] binfont load failed path=%s, fallback default font\n",
                           FONT_SERVICE_LV_PATH);
            return 50U;
        }
        g_ohos_external_font_stream = 0U;
    }

    g_ohos_external_font->fallback = LV_FONT_DEFAULT;
    esp_rom_printf("[P4-LVFONT] %s load ok path=%s version=%s size=%u\n",
                   g_ohos_external_font_stream ? "stream" : "binfont",
                   FONT_SERVICE_LV_PATH,
                   meta.version,
                   (unsigned)meta.size);
    return 0U;
}

uint32_t OhosLvglExternalFontIsLoaded(void)
{
    return g_ohos_external_font != NULL ? 1U : 0U;
}

const lv_font_t *OhosLvglExternalFontGet(void)
{
    return g_ohos_external_font;
}

void OhosLvglExternalFontRelease(void)
{
    if (g_ohos_external_font != NULL) {
        if (g_ohos_external_font_stream) {
            lv_font_stream_bin_destroy(g_ohos_external_font);
        } else {
            lv_binfont_destroy(g_ohos_external_font);
        }
        g_ohos_external_font = NULL;
        g_ohos_external_font_stream = 0U;
        esp_rom_printf("[P4-LVFONT] external font released\n");
    }
}

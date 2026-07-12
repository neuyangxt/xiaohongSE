#ifndef FONT_SERVICE_H
#define FONT_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT_SERVICE_DIR       "/data/system"
#define FONT_SERVICE_PATH      "/data/system/font.bin"
#define FONT_SERVICE_TMP_PATH  "/data/system/font.tmp"
#define FONT_SERVICE_META_PATH "/data/system/font.meta.json"
#define FONT_SERVICE_LV_PATH   "L:/system/font.bin"

#define FONT_SERVICE_VERSION_MAX 24U
#define FONT_SERVICE_FORMAT_MAX  16U
#define FONT_SERVICE_SHA256_MAX  65U

typedef struct {
    char version[FONT_SERVICE_VERSION_MAX];
    size_t size;
    char sha256[FONT_SERVICE_SHA256_MAX];
    char format[FONT_SERVICE_FORMAT_MAX];
    uint32_t valid;
    uint32_t updated_at;
} FontServiceMeta;

uint32_t FontServiceInit(void);
uint32_t FontServiceEnsureDemoFont(void);
uint32_t FontServiceInstallFontBuffer(const void *data, size_t len, const char *version, uint32_t updated_at);
uint32_t FontServiceLoadMeta(FontServiceMeta *out);
uint32_t FontServiceProbe(FontServiceMeta *out);
uint32_t FontServiceSelfTest(void);
uint32_t FontServiceStartTask(void);
uint32_t FontServiceIsReady(void);

#ifdef __cplusplus
}
#endif

#endif

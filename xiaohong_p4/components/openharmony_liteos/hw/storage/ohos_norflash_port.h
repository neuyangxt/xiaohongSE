#ifndef OHOS_NORFLASH_PORT_H
#define OHOS_NORFLASH_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OHOS_NORFLASH_ID_MAX_LEN 16U

typedef enum {
    OHOS_NORFLASH_ID_KIND_NONE = 0,
    OHOS_NORFLASH_ID_KIND_UNIQUE64 = 1,
    OHOS_NORFLASH_ID_KIND_JEDEC24 = 2,
} OhosNorFlashIdKind;

typedef struct {
    uint8_t bytes[OHOS_NORFLASH_ID_MAX_LEN];
    uint8_t len;
    uint8_t kind;
    uint32_t jedec_id;
    uint64_t unique_id;
    uint32_t flash_size;
    int32_t unique_ret;
    int32_t jedec_ret;
    int32_t size_ret;
} OhosNorFlashIdInfo;

uint32_t OhosNorFlashPortReadId(OhosNorFlashIdInfo *out);
const char *OhosNorFlashPortIdKindName(uint8_t kind);

#ifdef __cplusplus
}
#endif

#endif

#include "ohos_norflash_port.h"

#include <string.h>

#include "esp_err.h"
#include "esp_flash.h"
#include "esp_rom_sys.h"

static void OhosNorFlashStoreU64Be(uint8_t *out, uint64_t value)
{
    if (out == NULL) {
        return;
    }

    for (uint32_t i = 0; i < 8U; ++i) {
        out[i] = (uint8_t)((value >> ((7U - i) * 8U)) & 0xFFU);
    }
}

static void OhosNorFlashStoreJedecBe(uint8_t *out, uint32_t value)
{
    if (out == NULL) {
        return;
    }

    out[0] = (uint8_t)((value >> 16U) & 0xFFU);
    out[1] = (uint8_t)((value >> 8U) & 0xFFU);
    out[2] = (uint8_t)(value & 0xFFU);
}

const char *OhosNorFlashPortIdKindName(uint8_t kind)
{
    switch (kind) {
        case OHOS_NORFLASH_ID_KIND_UNIQUE64:
            return "UNIQUE64";
        case OHOS_NORFLASH_ID_KIND_JEDEC24:
            return "JEDEC24";
        case OHOS_NORFLASH_ID_KIND_NONE:
        default:
            return "NONE";
    }
}

uint32_t OhosNorFlashPortReadId(OhosNorFlashIdInfo *out)
{
    uint64_t uniqueId = 0;
    uint32_t jedecId = 0;
    uint32_t flashSize = 0;
    esp_err_t uniqueRet;
    esp_err_t jedecRet;
    esp_err_t sizeRet;

    if (out == NULL) {
        return 1U;
    }

    (void)memset(out, 0, sizeof(*out));
    out->unique_ret = (int32_t)ESP_FAIL;
    out->jedec_ret = (int32_t)ESP_FAIL;
    out->size_ret = (int32_t)ESP_FAIL;

    uniqueRet = esp_flash_read_unique_chip_id(esp_flash_default_chip, &uniqueId);
    out->unique_ret = (int32_t)uniqueRet;
    if (uniqueRet == ESP_OK) {
        out->unique_id = uniqueId;
        out->kind = (uint8_t)OHOS_NORFLASH_ID_KIND_UNIQUE64;
        out->len = 8U;
        OhosNorFlashStoreU64Be(out->bytes, uniqueId);
    }

    jedecRet = esp_flash_read_id(esp_flash_default_chip, &jedecId);
    out->jedec_ret = (int32_t)jedecRet;
    if (jedecRet == ESP_OK) {
        out->jedec_id = jedecId;
        if (out->len == 0U) {
            out->kind = (uint8_t)OHOS_NORFLASH_ID_KIND_JEDEC24;
            out->len = 3U;
            OhosNorFlashStoreJedecBe(out->bytes, jedecId);
        }
    }

    sizeRet = esp_flash_get_size(esp_flash_default_chip, &flashSize);
    out->size_ret = (int32_t)sizeRet;
    if (sizeRet == ESP_OK) {
        out->flash_size = flashSize;
    }

    esp_rom_printf("[P4-NORFLASH] read id kind=%s len=%u uniqueRet=%d unique=0x%08x%08x jedecRet=%d jedec=0x%06x sizeRet=%d size=%u\n",
                   OhosNorFlashPortIdKindName(out->kind),
                   out->len,
                   out->unique_ret,
                   (uint32_t)(uniqueId >> 32U),
                   (uint32_t)(uniqueId & 0xFFFFFFFFULL),
                   out->jedec_ret,
                   out->jedec_id,
                   out->size_ret,
                   out->flash_size);

    return (out->len > 0U) ? 0U : 2U;
}

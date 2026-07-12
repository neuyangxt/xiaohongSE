#include "device_identity_service.h"

#include <string.h>

#include "esp_rom_sys.h"

static uint8_t g_device_id_raw[DEVICE_ID_MAX_LEN];
static uint32_t g_device_id_len = 0;
static char g_device_id_hex[(DEVICE_ID_MAX_LEN * 2U) + 1U];
static uint32_t g_device_id_init_done = 0;
static uint32_t g_device_id_ready = 0;
static OhosNorFlashIdInfo g_device_id_flash_info;

static char DeviceIdentityNibbleToHex(uint8_t value)
{
    value &= 0x0FU;
    return (value < 10U) ? (char)('0' + value) : (char)('A' + (value - 10U));
}

static void DeviceIdentityBuildHex(void)
{
    for (uint32_t i = 0; i < g_device_id_len; ++i) {
        g_device_id_hex[i * 2U] = DeviceIdentityNibbleToHex((uint8_t)(g_device_id_raw[i] >> 4U));
        g_device_id_hex[(i * 2U) + 1U] = DeviceIdentityNibbleToHex(g_device_id_raw[i]);
    }
    g_device_id_hex[g_device_id_len * 2U] = '\0';
}

uint32_t DeviceIdentityInit(void)
{
    uint32_t ret;

    if (g_device_id_init_done) {
        return g_device_id_ready ? 0U : 1U;
    }

    g_device_id_init_done = 1U;
    ret = OhosNorFlashPortReadId(&g_device_id_flash_info);
    if (ret != 0U || g_device_id_flash_info.len == 0U ||
        g_device_id_flash_info.len > DEVICE_ID_MAX_LEN) {
        g_device_id_ready = 0;
        g_device_id_len = 0;
        g_device_id_hex[0] = '\0';
        esp_rom_printf("[P4-DEVICE-ID] init failed ret=%u kind=%s len=%u uniqueRet=%d jedecRet=%d\n",
                       ret,
                       OhosNorFlashPortIdKindName(g_device_id_flash_info.kind),
                       g_device_id_flash_info.len,
                       g_device_id_flash_info.unique_ret,
                       g_device_id_flash_info.jedec_ret);
        return 1U;
    }

    (void)memcpy(g_device_id_raw, g_device_id_flash_info.bytes, g_device_id_flash_info.len);
    g_device_id_len = g_device_id_flash_info.len;
    DeviceIdentityBuildHex();
    g_device_id_ready = 1U;

    esp_rom_printf("[P4-DEVICE-ID] init ok kind=%s len=%u hex=%s jedec=0x%06x size=%u\n",
                   OhosNorFlashPortIdKindName(g_device_id_flash_info.kind),
                   g_device_id_len,
                   g_device_id_hex,
                   g_device_id_flash_info.jedec_id,
                   g_device_id_flash_info.flash_size);

    return 0U;
}

uint32_t DeviceIdentityGetRaw(uint8_t *buf, uint32_t bufLen, uint32_t *outLen)
{
    uint32_t ret;

    if (outLen != NULL) {
        *outLen = 0;
    }

    ret = DeviceIdentityInit();
    if (ret != 0U || !g_device_id_ready) {
        return 1U;
    }

    if (buf == NULL || outLen == NULL || bufLen < g_device_id_len) {
        return 2U;
    }

    (void)memcpy(buf, g_device_id_raw, g_device_id_len);
    *outLen = g_device_id_len;
    return 0U;
}

const char *DeviceIdentityGetHex(void)
{
    if (DeviceIdentityInit() != 0U || !g_device_id_ready) {
        return "";
    }

    return g_device_id_hex;
}

uint32_t DeviceIdentityIsReady(void)
{
    return (DeviceIdentityInit() == 0U && g_device_id_ready) ? 1U : 0U;
}

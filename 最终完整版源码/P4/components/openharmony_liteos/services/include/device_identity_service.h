#ifndef DEVICE_IDENTITY_SERVICE_H
#define DEVICE_IDENTITY_SERVICE_H

#include <stdint.h>

#include "ohos_norflash_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_ID_MAX_LEN OHOS_NORFLASH_ID_MAX_LEN

uint32_t DeviceIdentityInit(void);
uint32_t DeviceIdentityGetRaw(uint8_t *buf, uint32_t bufLen, uint32_t *outLen);
const char *DeviceIdentityGetHex(void);
uint32_t DeviceIdentityIsReady(void);

#ifdef __cplusplus
}
#endif

#endif

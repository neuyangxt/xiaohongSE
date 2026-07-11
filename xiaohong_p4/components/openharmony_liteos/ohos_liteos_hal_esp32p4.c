#include "esp_log.h"

static const char *TAG = "ohos_hal";

/*
 * Bring-up shim only.
 * Final implementation must map these to ESP32-P4 interrupt controller APIs.
 */
void HalSetLocalInterPri(unsigned int hwiNum, unsigned char priority)
{
    ESP_EARLY_LOGD(TAG, "HalSetLocalInterPri(%u, %u)", hwiNum, priority);
    (void)hwiNum;
    (void)priority;
}

void HalIrqEnable(unsigned int hwiNum)
{
    ESP_EARLY_LOGD(TAG, "HalIrqEnable(%u)", hwiNum);
    (void)hwiNum;
}

void HalIrqDisable(unsigned int hwiNum)
{
    ESP_EARLY_LOGD(TAG, "HalIrqDisable(%u)", hwiNum);
    (void)hwiNum;
}

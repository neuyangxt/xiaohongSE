#include "file_service.h"

#include "esp_log.h"
#include "ohos_lfs_port.h"

#define OHOS_FILE_TAG "OHOS-FILE"

uint32_t OhosFileServiceInit(void)
{
    ESP_LOGI(OHOS_FILE_TAG, "FileService init start");
    uint32_t ret = OhosLfsPortInit();
    ESP_LOGI(OHOS_FILE_TAG, "FileService init returned: %u", (unsigned)ret);
    return ret;
}

uint32_t OhosFileServiceSelfTest(void)
{
    ESP_LOGI(OHOS_FILE_TAG, "FileService selftest start");

    uint32_t ret = OhosLfsPortSelfTest();
    if (ret == 0) {
        ESP_LOGI(OHOS_FILE_TAG, "FileService selftest success");
    } else {
        ESP_LOGE(OHOS_FILE_TAG, "FileService selftest failed ret=%u", (unsigned)ret);
    }

    return ret;
}

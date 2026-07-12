#include "ohos_board_i2c.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_system.h"
#include "ohos_liteos_bridge.h"

static const char *TAG = "ohos_app";

extern int ohos_liteos_kernelinit_task_probe(void);
extern int ohos_liteos_bringup(bool start_scheduler);

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-IDF app_main entered");
    ESP_LOGI(TAG, "reset reason: %d", esp_reset_reason());

    ESP_LOGI(TAG, "Start OpenHarmony LiteOS-M full scheduler takeover");
    int ret = ohos_liteos_bringup(true);
    ESP_LOGE(TAG, "OpenHarmony LiteOS-M scheduler returned unexpectedly: %d", ret);
}

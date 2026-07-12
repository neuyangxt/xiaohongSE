#ifndef OHOS_BOARD_I2C_H
#define OHOS_BOARD_I2C_H

#include "driver/i2c_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OHOS_BOARD_I2C_PORT      I2C_NUM_0
#define OHOS_BOARD_I2C_SCL_IO    GPIO_NUM_8
#define OHOS_BOARD_I2C_SDA_IO    GPIO_NUM_7
#define OHOS_BOARD_I2C_FREQ_HZ   100000

/* Final product board reset pins, based on current schematic review. */
#define OHOS_BOARD_TP_RST_IO     GPIO_NUM_6
#define OHOS_BOARD_CSI_RST_IO    GPIO_NUM_26

esp_err_t OhosBoardPeripheralsResetForFinalBoard(void);
void OhosBoardI2cScanSharedBus(void);
esp_err_t OhosBoardI2cGetSharedBus(i2c_master_bus_handle_t *out_bus);
i2c_master_bus_handle_t OhosBoardI2cGetCachedBus(void);

#ifdef __cplusplus
}
#endif

#endif

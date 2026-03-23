#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t doers3_i2c_acquire(void);
esp_err_t doers3_i2c_release(void);
esp_err_t doers3_i2c_get(i2c_master_bus_handle_t *out_bus);

#ifdef __cplusplus
}
#endif

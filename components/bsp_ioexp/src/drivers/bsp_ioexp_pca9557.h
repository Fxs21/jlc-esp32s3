#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t output_default;
    uint8_t direction_mask;
} bsp_ioexp_pca9557_cfg_t;

typedef struct bsp_ioexp_pca9557_s *bsp_ioexp_pca9557_handle_t;

esp_err_t bsp_ioexp_pca9557_open(const bsp_ioexp_pca9557_cfg_t *cfg, bsp_ioexp_pca9557_handle_t *out_handle);
esp_err_t bsp_ioexp_pca9557_close(bsp_ioexp_pca9557_handle_t handle);
esp_err_t bsp_ioexp_pca9557_set_level(bsp_ioexp_pca9557_handle_t handle, uint8_t pin, bool active_high, bool asserted);

#ifdef __cplusplus
}
#endif

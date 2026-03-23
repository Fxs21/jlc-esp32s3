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
} pca9557_config_t;

typedef struct pca9557_s *pca9557_handle_t;

esp_err_t pca9557_open(const pca9557_config_t *cfg, pca9557_handle_t *out_handle);
esp_err_t pca9557_close(pca9557_handle_t handle);
esp_err_t pca9557_write_pin(pca9557_handle_t handle, uint8_t pin, bool level);

#ifdef __cplusplus
}
#endif

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
} tca9554_config_t;

typedef struct tca9554_s *tca9554_handle_t;

esp_err_t tca9554_open(const tca9554_config_t *cfg, tca9554_handle_t *out_handle);
esp_err_t tca9554_close(tca9554_handle_t handle);
esp_err_t tca9554_read_port(tca9554_handle_t handle, uint8_t *value);
esp_err_t tca9554_write_port(tca9554_handle_t handle, uint8_t value);
esp_err_t tca9554_write_port_masked(tca9554_handle_t handle, uint8_t mask, uint8_t value);
esp_err_t tca9554_read_pin(tca9554_handle_t handle, uint8_t pin, bool *level);
esp_err_t tca9554_write_pin(tca9554_handle_t handle, uint8_t pin, bool level);

#ifdef __cplusplus
}
#endif

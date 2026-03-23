#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    uint8_t glitch_ignore_cnt;
    bool enable_internal_pullup;
} bsp_i2c_bus_cfg_t;

bsp_i2c_bus_cfg_t bsp_i2c_bus_default_cfg(void);

esp_err_t bsp_i2c_bus_open(const bsp_i2c_bus_cfg_t *cfg);
esp_err_t bsp_i2c_bus_close(void);

esp_err_t bsp_i2c_bus_get(i2c_master_bus_handle_t *out_bus);
bool bsp_i2c_bus_is_ready(void);

#ifdef __cplusplus
}
#endif

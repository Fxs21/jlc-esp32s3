#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *tag;
    i2c_port_num_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    uint8_t glitch_ignore_cnt;
    bool internal_pullup;
} bsp_i2c_bus_config_t;

typedef struct bsp_i2c_bus *bsp_i2c_bus_handle_t;

esp_err_t bsp_i2c_bus_acquire(bsp_i2c_bus_handle_t *handle, const bsp_i2c_bus_config_t *config);
esp_err_t bsp_i2c_bus_release(bsp_i2c_bus_handle_t *handle);
esp_err_t bsp_i2c_bus_get(bsp_i2c_bus_handle_t handle, i2c_master_bus_handle_t *out_bus);
esp_err_t bsp_i2c_bus_probe(bsp_i2c_bus_handle_t handle, uint8_t address, uint32_t timeout_ms);
esp_err_t bsp_i2c_bus_scan(bsp_i2c_bus_handle_t handle, uint8_t *out_addresses, size_t address_capacity,
                           size_t *out_count, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

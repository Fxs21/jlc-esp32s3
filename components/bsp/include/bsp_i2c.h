#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_I2C_SCAN_FIRST_ADDR 0x03u
#define BSP_I2C_SCAN_LAST_ADDR  0x77u
#define BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS 50u

// Board-specific I2C bus configuration, provided by each board port.
typedef struct {
    i2c_port_num_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    uint8_t glitch_ignore_cnt;
    bool internal_pullup;
} bsp_i2c_bus_config_t;

// Acquire the board I2C bus. On the first call the bus is created and
// initialised. The returned handle is written to *out_bus.
// Each acquire must be paired with bsp_i2c_release().
esp_err_t bsp_i2c_acquire(i2c_master_bus_handle_t *out_bus);

// Release one reference. On the last release the bus is torn down.
esp_err_t bsp_i2c_release(void);

// Probe an address on the board I2C bus.
// When timeout_ms is 0, the default (BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS) is used.
esp_err_t bsp_i2c_probe(uint8_t address, uint32_t timeout_ms);

// Scan the board I2C bus. out_count receives the total number of detected
// devices, even when the address buffer is smaller than the count.
esp_err_t bsp_i2c_scan(uint8_t *out_addresses, size_t address_capacity,
                       size_t *out_count, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

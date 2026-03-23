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

// Probe an address on the board default I2C bus using BSP-owned pins and lifecycle.
esp_err_t bsp_i2c_probe(uint8_t address, uint32_t timeout_ms);

// Scan the board default I2C bus using BSP-owned pins and lifecycle.
// out_count receives the total number of detected devices, even when the address
// buffer is smaller than the number of devices found.
// Returns ESP_ERR_INVALID_SIZE when the buffer was too small and out_count reports
// the full detected count.
esp_err_t bsp_i2c_scan(uint8_t *out_addresses, size_t address_capacity, size_t *out_count, uint32_t timeout_ms);

// Compatibility helper for callers that only need the default diagnostic scan timeout.
static inline esp_err_t bsp_i2c_scan_default(uint8_t *out_addresses, size_t address_capacity, size_t *out_count)
{
    return bsp_i2c_scan(out_addresses, address_capacity, out_count, BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS);
}

// ESP-IDF escape hatch for app-owned devices on the board default I2C bus.
// The caller must pair every successful acquire with bsp_i2c_release_default().
// On failure, *out_bus is set to NULL.
esp_err_t bsp_i2c_acquire_default(i2c_master_bus_handle_t *out_bus);
esp_err_t bsp_i2c_release_default(void);

#ifdef __cplusplus
}
#endif

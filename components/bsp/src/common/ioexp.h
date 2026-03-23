#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Board-provided driver.  init/deinit are called by acquire/release on
// first/last reference.  write_pin/read_pin may be NULL if not supported.
typedef struct {
    esp_err_t (*init)(void *ctx);
    void      (*deinit)(void *ctx);
    esp_err_t (*write_pin)(void *ctx, uint8_t pin, bool level);
    esp_err_t (*read_pin)(void *ctx, uint8_t pin, bool *out_level);
    void *ctx;
} bsp_ioexp_driver_t;

// Acquire the IO expander.  On first acquire, calls driver->init().
// Returns ESP_OK on success.  Call bsp_ioexp_release() to drop the reference.
esp_err_t bsp_ioexp_acquire(const bsp_ioexp_driver_t *driver);

// Release one reference.  When the last reference is dropped, calls
// driver->deinit() and tears down the lock.
esp_err_t bsp_ioexp_release(void);

// Write a single pin.  Returns ESP_ERR_INVALID_STATE if not acquired.
esp_err_t bsp_ioexp_set_pin(uint8_t pin, bool level);

// Read a single pin.  Returns ESP_ERR_INVALID_STATE if not acquired.
esp_err_t bsp_ioexp_get_pin(uint8_t pin, bool *out_level);

#ifdef __cplusplus
}
#endif

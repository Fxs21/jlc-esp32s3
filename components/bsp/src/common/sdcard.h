#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t clk;
    gpio_num_t cmd;
    gpio_num_t d0;
} bsp_sdcard_pins_t;

// Provided by each board port. Called once per mount.
const bsp_sdcard_pins_t *bsp_sdcard_get_pins(void);

#ifdef __cplusplus
}
#endif

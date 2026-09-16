#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// Board-specific I2C bus truth: port, pins, pull-up and glitch filter.
//
// Internal to components/bsp. Board ports implement bsp_i2c_get_config() and
// src/common/bsp_i2c.c consumes it on the first bsp_i2c_acquire().
// Bus pins are board truth, so they stay out of the public BSP API.
typedef struct {
    i2c_port_num_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    uint8_t glitch_ignore_cnt;
    bool internal_pullup;
} bsp_i2c_bus_config_t;

const bsp_i2c_bus_config_t *bsp_i2c_get_config(void);

#ifdef __cplusplus
}
#endif

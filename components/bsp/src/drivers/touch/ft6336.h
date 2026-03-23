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
    uint16_t x;
    uint16_t y;
    uint8_t pressure;
    uint8_t id;
} ft6336_point_t;

typedef struct {
    i2c_master_bus_handle_t bus;
    uint8_t address;
    uint16_t x_max;
    uint16_t y_max;
    gpio_num_t reset_gpio;
    gpio_num_t int_gpio;
    uint8_t level_reset;
    uint8_t level_interrupt;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} ft6336_config_t;

typedef struct ft6336_s *ft6336_handle_t;

esp_err_t ft6336_open(const ft6336_config_t *cfg, ft6336_handle_t *out_handle);
esp_err_t ft6336_close(ft6336_handle_t handle);
esp_err_t ft6336_read(ft6336_handle_t handle, ft6336_point_t *points, size_t points_capacity, size_t *points_num);

#ifdef __cplusplus
}
#endif

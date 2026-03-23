#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

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
} ft5x06_config_t;

typedef struct ft5x06_s *ft5x06_handle_t;

esp_err_t ft5x06_open(const ft5x06_config_t *cfg, ft5x06_handle_t *out_handle);
esp_err_t ft5x06_close(ft5x06_handle_t handle);
esp_err_t ft5x06_read(ft5x06_handle_t handle, esp_lcd_touch_point_data_t *points, size_t points_capacity, size_t *points_num);

#ifdef __cplusplus
}
#endif

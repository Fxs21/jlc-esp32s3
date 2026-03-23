#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
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
} bsp_touch_ft5x06_cfg_t;

typedef struct bsp_touch_driver_s *bsp_touch_driver_handle_t;

esp_err_t bsp_touch_ft5x06_new(const bsp_touch_ft5x06_cfg_t *cfg, bsp_touch_driver_handle_t *out_handle);
esp_err_t bsp_touch_ft5x06_del(bsp_touch_driver_handle_t handle);
esp_err_t bsp_touch_ft5x06_read(bsp_touch_driver_handle_t handle,
                                esp_lcd_touch_point_data_t *points,
                                size_t points_capacity,
                                size_t *points_num);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t gpio;
    bool output_invert;
} bsp_backlight_ledc_cfg_t;

typedef struct bsp_backlight_driver_s *bsp_backlight_driver_handle_t;

esp_err_t bsp_backlight_ledc_new(const bsp_backlight_ledc_cfg_t *cfg, bsp_backlight_driver_handle_t *out_handle);
esp_err_t bsp_backlight_ledc_del(bsp_backlight_driver_handle_t handle);
esp_err_t bsp_backlight_ledc_set_percent(bsp_backlight_driver_handle_t handle, uint8_t percent);

#ifdef __cplusplus
}
#endif

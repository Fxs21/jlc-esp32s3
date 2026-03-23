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
} backlight_ledc_config_t;

typedef struct backlight_ledc_s *backlight_ledc_handle_t;

esp_err_t backlight_ledc_open(const backlight_ledc_config_t *cfg, backlight_ledc_handle_t *out_handle);
esp_err_t backlight_ledc_close(backlight_ledc_handle_t handle);
esp_err_t backlight_ledc_set_percent(backlight_ledc_handle_t handle, uint8_t percent);

#ifdef __cplusplus
}
#endif

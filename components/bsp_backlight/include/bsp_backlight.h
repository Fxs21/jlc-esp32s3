#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_backlight_s *bsp_backlight_handle_t;

typedef struct {
    bool present;
} bsp_backlight_desc_t;

const bsp_backlight_desc_t *bsp_backlight_get_desc(void);

esp_err_t bsp_backlight_open(bsp_backlight_handle_t *out_handle);
esp_err_t bsp_backlight_close(bsp_backlight_handle_t handle);
esp_err_t bsp_backlight_set_percent(bsp_backlight_handle_t handle, uint8_t percent);
esp_err_t bsp_backlight_get_percent(bsp_backlight_handle_t handle, uint8_t *out_percent);

#ifdef __cplusplus
}
#endif

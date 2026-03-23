#pragma once

#include <stdint.h>

#include "bsp_backlight.h"
#include "bsp_touch.h"
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Application UI entry point: LVGL display, touch, and backlight control.
typedef struct bsp_ui_s *bsp_ui_handle_t;

// On failure, *out_handle is set to NULL after out_handle is validated.
esp_err_t bsp_ui_open(bsp_ui_handle_t *out_handle);
esp_err_t bsp_ui_process(bsp_ui_handle_t handle, uint32_t *out_delay_ms);
lv_display_t *bsp_ui_get_lvgl_display(bsp_ui_handle_t handle);
lv_indev_t *bsp_ui_get_lvgl_indev(bsp_ui_handle_t handle);
esp_err_t bsp_ui_set_backlight(bsp_ui_handle_t handle, uint8_t percent);
esp_err_t bsp_ui_close(bsp_ui_handle_t handle);

#ifdef __cplusplus
}
#endif

#pragma once

#include "bsp_display.h"
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_display_port_lvgl_open(bsp_display_handle_t handle, lv_display_t **out_display);
esp_err_t bsp_display_port_lvgl_close(bsp_display_handle_t handle, lv_display_t *display);

#ifdef __cplusplus
}
#endif

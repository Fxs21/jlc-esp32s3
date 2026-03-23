#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_display_s *bsp_display_handle_t;

typedef struct {
    bool present;
    bool has_backlight;
    bool has_touch;
    uint16_t width;
    uint16_t height;
    lcd_rgb_data_endian_t data_endian;
} bsp_display_desc_t;

const bsp_display_desc_t *bsp_display_get_desc(void);

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle);
esp_err_t bsp_display_close(bsp_display_handle_t handle);
esp_err_t bsp_display_draw_bitmap(bsp_display_handle_t handle,
                                  int x_start,
                                  int y_start,
                                  int x_end,
                                  int y_end,
                                  const void *color_data);
esp_err_t bsp_display_fill(bsp_display_handle_t handle, uint16_t color);
esp_err_t bsp_display_register_flush_done_cb(bsp_display_handle_t handle,
                                             esp_lcd_panel_io_color_trans_done_cb_t cb,
                                             void *user_ctx);

#ifdef __cplusplus
}
#endif

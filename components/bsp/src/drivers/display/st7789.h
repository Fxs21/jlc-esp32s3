#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*st7789_select_cb_t)(void *user_ctx, bool selected);

typedef struct {
    uint16_t width;
    uint16_t height;
    lcd_rgb_data_endian_t data_endian;
    bool invert_color;
    spi_host_device_t spi_host;
    gpio_num_t cs_gpio;
    gpio_num_t dc_gpio;
    gpio_num_t mosi_gpio;
    gpio_num_t sclk_gpio;
    gpio_num_t reset_gpio;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
    st7789_select_cb_t select_cb;
    void *select_ctx;
} st7789_config_t;

typedef struct st7789_s *st7789_handle_t;

esp_err_t st7789_open(const st7789_config_t *cfg, st7789_handle_t *out_handle);
esp_err_t st7789_close(st7789_handle_t handle);
esp_err_t st7789_draw_bitmap(st7789_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data);
esp_err_t st7789_flush_wait(st7789_handle_t handle);
esp_err_t st7789_register_flush_done_cb(st7789_handle_t handle, esp_lcd_panel_io_color_trans_done_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif

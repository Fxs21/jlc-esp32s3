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

typedef enum {
    BSP_DISPLAY_CTRL_PIN_NONE = 0,
    BSP_DISPLAY_CTRL_PIN_GPIO,
    BSP_DISPLAY_CTRL_PIN_IOEXP,
} bsp_display_ctrl_pin_type_t;

typedef struct {
    bsp_display_ctrl_pin_type_t type;
    bool active_high;
    union {
        gpio_num_t gpio_num;
        uint8_t ioexp_num;
    } num;
} bsp_display_ctrl_pin_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    lcd_rgb_data_endian_t data_endian;
    bool invert_color;
    spi_host_device_t spi_host;
    bsp_display_ctrl_pin_t lcd_cs;
    gpio_num_t lcd_dc;
    gpio_num_t lcd_mosi;
    gpio_num_t lcd_sclk;
    gpio_num_t panel_reset_gpio;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} bsp_display_st7789_cfg_t;

typedef struct bsp_display_driver_s *bsp_display_driver_handle_t;

esp_err_t bsp_display_st7789_new(const bsp_display_st7789_cfg_t *cfg, bsp_display_driver_handle_t *out_handle);
esp_err_t bsp_display_st7789_del(bsp_display_driver_handle_t handle);
esp_err_t bsp_display_st7789_draw_bitmap(bsp_display_driver_handle_t handle,
                                         int x_start,
                                         int y_start,
                                         int x_end,
                                         int y_end,
                                         const void *color_data);
esp_err_t bsp_display_st7789_flush_wait(bsp_display_driver_handle_t handle);
esp_err_t bsp_display_st7789_register_flush_done_cb(bsp_display_driver_handle_t handle,
                                                    esp_lcd_panel_io_color_trans_done_cb_t cb,
                                                    void *user_ctx);

#ifdef __cplusplus
}
#endif

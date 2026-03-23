#include "bsp_display_backend.h"

#include "bsp_boarddb.h"
#include "bsp_display_st7789.h"

static bsp_display_ctrl_pin_t ctrl_pin_from_boarddb(bsp_boarddb_ctrl_pin_t pin)
{
    bsp_display_ctrl_pin_t out = {
        .type = (bsp_display_ctrl_pin_type_t)pin.type,
        .active_high = pin.active_high,
    };
    if (pin.type == BSP_BOARDDB_CTRL_PIN_GPIO) {
        out.num.gpio_num = pin.num.gpio_num;
    } else {
        out.num.ioexp_num = pin.num.ioexp_num;
    }
    return out;
}

static esp_err_t st7789_create(const void *config, void **out_ctx)
{
    bsp_display_driver_handle_t handle = NULL;
    esp_err_t ret = bsp_display_st7789_new((const bsp_display_st7789_cfg_t *)config, &handle);
    if (ret == ESP_OK) {
        *out_ctx = handle;
    }
    return ret;
}

static esp_err_t st7789_destroy(void *ctx)
{
    return bsp_display_st7789_del((bsp_display_driver_handle_t)ctx);
}

static esp_err_t st7789_draw_bitmap(void *ctx, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    return bsp_display_st7789_draw_bitmap((bsp_display_driver_handle_t)ctx, x_start, y_start, x_end, y_end, color_data);
}

static esp_err_t st7789_flush_wait(void *ctx)
{
    return bsp_display_st7789_flush_wait((bsp_display_driver_handle_t)ctx);
}

static esp_err_t st7789_register_flush_done_cb(void *ctx, esp_lcd_panel_io_color_trans_done_cb_t cb, void *user_ctx)
{
    return bsp_display_st7789_register_flush_done_cb((bsp_display_driver_handle_t)ctx, cb, user_ctx);
}

const bsp_display_backend_t *bsp_display_backend_get(void)
{
    static bsp_display_st7789_cfg_t st7789_cfg;
    static bsp_display_backend_t backend;
    static bool initialized;

    const bsp_boarddb_display_desc_t *display = &bsp_boarddb_get()->display;
    if (!display->present) {
        return NULL;
    }

    switch (display->kind) {
    case BSP_BOARDDB_DISPLAY_KIND_ST7789:
        if (!initialized) {
            st7789_cfg.width = display->width;
            st7789_cfg.height = display->height;
            st7789_cfg.data_endian = display->data_endian;
            st7789_cfg.invert_color = display->invert_color;
            st7789_cfg.spi_host = display->spi_host;
            st7789_cfg.lcd_cs = ctrl_pin_from_boarddb(display->lcd_cs);
            st7789_cfg.lcd_dc = display->lcd_dc;
            st7789_cfg.lcd_mosi = display->lcd_mosi;
            st7789_cfg.lcd_sclk = display->lcd_sclk;
            st7789_cfg.panel_reset_gpio = display->panel_reset_gpio;
            st7789_cfg.swap_xy = display->swap_xy;
            st7789_cfg.mirror_x = display->mirror_x;
            st7789_cfg.mirror_y = display->mirror_y;

            backend.config = &st7789_cfg;
            backend.create = st7789_create;
            backend.destroy = st7789_destroy;
            backend.draw_bitmap = st7789_draw_bitmap;
            backend.flush_wait = st7789_flush_wait;
            backend.register_flush_done_cb = st7789_register_flush_done_cb;
            initialized = true;
        }
        return &backend;
    case BSP_BOARDDB_DISPLAY_KIND_NONE:
    default:
        return NULL;
    }
}

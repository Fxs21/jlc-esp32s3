#include <stdlib.h>

#include "bsp_display_st7789.h"
#include "bsp_ioexp.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#define BSP_DISPLAY_TAG "bsp_display"

#define LCD_PIXEL_CLOCK_HZ    (80 * 1000 * 1000)
#define LCD_CMD_BITS          8
#define LCD_PARAM_BITS        8
#define LCD_SPI_MODE          2
#define LCD_TRANS_QUEUE_DEPTH 10
#define LCD_BITS_PER_PIXEL    16

struct bsp_display_driver_s {
    const bsp_display_st7789_cfg_t *cfg;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
    bool ioexp_acquired;
    bool spi_inited;
    bool flush_done_cb_registered;
};

static inline bsp_ioexp_pin_t to_ioexp_pin(bsp_display_ctrl_pin_t pin)
{
    return (bsp_ioexp_pin_t) {
        .pin = pin.num.ioexp_num,
        .active_high = pin.active_high,
    };
}

static inline bool has_gpio_cs(const bsp_display_st7789_cfg_t *cfg)
{
    return cfg->lcd_cs.type == BSP_DISPLAY_CTRL_PIN_GPIO && cfg->lcd_cs.num.gpio_num != GPIO_NUM_NC;
}

static inline bool has_ioexp_cs(const bsp_display_st7789_cfg_t *cfg)
{
    return cfg->lcd_cs.type == BSP_DISPLAY_CTRL_PIN_IOEXP;
}

static esp_err_t spi_bus_init(struct bsp_display_driver_s *ctx)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = ctx->cfg->lcd_mosi,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = ctx->cfg->lcd_sclk,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = ctx->cfg->width * ctx->cfg->height * sizeof(uint16_t),
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    esp_err_t ret = spi_bus_initialize(ctx->cfg->spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK) {
        ctx->spi_inited = true;
    }
    return ret;
}

static esp_err_t panel_init(struct bsp_display_driver_s *ctx)
{
    esp_err_t ret = spi_bus_init(ctx);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_lcd_panel_io_spi_config_t panel_io_cfg = {
        .dc_gpio_num = ctx->cfg->lcd_dc,
        .cs_gpio_num = GPIO_NUM_NC,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = LCD_SPI_MODE,
        .trans_queue_depth = LCD_TRANS_QUEUE_DEPTH,
    };
    if (has_gpio_cs(ctx->cfg)) {
        panel_io_cfg.cs_gpio_num = ctx->cfg->lcd_cs.num.gpio_num;
    }

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)ctx->cfg->spi_host, &panel_io_cfg, &ctx->panel_io);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num = ctx->cfg->panel_reset_gpio,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .data_endian = ctx->cfg->data_endian,
    };
    ret = esp_lcd_new_panel_st7789(ctx->panel_io, &panel_dev_cfg, &ctx->panel);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_lcd_panel_reset(ctx->panel);
    if (ret != ESP_OK) {
        return ret;
    }

    if (has_ioexp_cs(ctx->cfg)) {
        ret = bsp_ioexp_set_level(to_ioexp_pin(ctx->cfg->lcd_cs), true);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ret = esp_lcd_panel_init(ctx->panel);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ctx->cfg->invert_color) {
        ret = esp_lcd_panel_invert_color(ctx->panel, true);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    ret = esp_lcd_panel_swap_xy(ctx->panel, ctx->cfg->swap_xy);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_lcd_panel_mirror(ctx->panel, ctx->cfg->mirror_x, ctx->cfg->mirror_y);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_lcd_panel_disp_on_off(ctx->panel, true);
}

esp_err_t bsp_display_st7789_new(const bsp_display_st7789_cfg_t *cfg, bsp_display_driver_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "out_handle is null");

    struct bsp_display_driver_s *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_NO_MEM, BSP_DISPLAY_TAG, "no memory");
    ctx->cfg = cfg;

    if (has_ioexp_cs(cfg)) {
        esp_err_t ret = bsp_ioexp_open();
        if (ret != ESP_OK) {
            free(ctx);
            return ret;
        }
        ctx->ioexp_acquired = true;
    }

    esp_err_t ret = panel_init(ctx);
    if (ret != ESP_OK) {
        (void)bsp_display_st7789_del(ctx);
        return ret;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t bsp_display_st7789_del(bsp_display_driver_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    struct bsp_display_driver_s *ctx = handle;
    esp_err_t first_err = ESP_OK;

    if (ctx->panel != NULL) {
        esp_err_t ret = esp_lcd_panel_del(ctx->panel);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (ctx->panel_io != NULL) {
        esp_err_t ret = esp_lcd_panel_io_del(ctx->panel_io);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (ctx->spi_inited) {
        esp_err_t ret = spi_bus_free(ctx->cfg->spi_host);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (ctx->ioexp_acquired) {
        esp_err_t ret = bsp_ioexp_close();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }

    free(ctx);
    return first_err;
}

esp_err_t bsp_display_st7789_draw_bitmap(bsp_display_driver_handle_t handle,
                                         int x_start,
                                         int y_start,
                                         int x_end,
                                         int y_end,
                                         const void *color_data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(color_data != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "color_data is null");
    return esp_lcd_panel_draw_bitmap(handle->panel, x_start, y_start, x_end, y_end, color_data);
}

esp_err_t bsp_display_st7789_flush_wait(bsp_display_driver_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    return esp_lcd_panel_io_tx_param(handle->panel_io, -1, NULL, 0);
}

esp_err_t bsp_display_st7789_register_flush_done_cb(bsp_display_driver_handle_t handle,
                                                    esp_lcd_panel_io_color_trans_done_cb_t cb,
                                                    void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    if (cb != NULL && handle->flush_done_cb_registered) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = cb,
    };
    esp_err_t ret = esp_lcd_panel_io_register_event_callbacks(handle->panel_io, &cbs, user_ctx);
    if (ret == ESP_OK) {
        handle->flush_done_cb_registered = (cb != NULL);
    }
    return ret;
}

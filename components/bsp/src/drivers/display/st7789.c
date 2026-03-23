#include "st7789.h"

#include <stdlib.h>

#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "st7789"
#define LCD_PIXEL_CLOCK_HZ    (80 * 1000 * 1000)
#define LCD_CMD_BITS          8
#define LCD_PARAM_BITS        8
#define LCD_SPI_MODE          2
#define LCD_TRANS_QUEUE_DEPTH 10
#define LCD_BITS_PER_PIXEL    16

struct st7789_s {
    const st7789_config_t *cfg;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t transfer_done_sem;
    bool spi_inited;
    esp_lcd_panel_io_color_trans_done_cb_t transfer_done_cb;
    void *transfer_done_user_ctx;
};

static bool color_trans_done_bridge_cb(esp_lcd_panel_io_handle_t panel_io,
                                       esp_lcd_panel_io_event_data_t *edata,
                                       void *user_ctx)
{
    struct st7789_s *ctx = (struct st7789_s *)user_ctx;
    BaseType_t task_woken = pdFALSE;
    if (ctx != NULL && ctx->transfer_done_sem != NULL) {
        xSemaphoreGiveFromISR(ctx->transfer_done_sem, &task_woken);
    }
    bool need_yield = task_woken == pdTRUE;
    if (ctx == NULL || ctx->transfer_done_cb == NULL) {
        return need_yield;
    }
    return ctx->transfer_done_cb(panel_io, edata, ctx->transfer_done_user_ctx) || need_yield;
}

static esp_err_t init_spi_bus(struct st7789_s *ctx)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = ctx->cfg->mosi_gpio,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = ctx->cfg->sclk_gpio,
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

esp_err_t st7789_open(const st7789_config_t *cfg, st7789_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    struct st7789_s *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    ctx->cfg = cfg;
    ctx->transfer_done_sem = xSemaphoreCreateBinary();
    if (ctx->transfer_done_sem == NULL) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = init_spi_bus(ctx);
    if (ret != ESP_OK) {
        vSemaphoreDelete(ctx->transfer_done_sem);
        free(ctx);
        return ret;
    }

    esp_lcd_panel_io_spi_config_t panel_io_cfg = {
        .dc_gpio_num = cfg->dc_gpio,
        .cs_gpio_num = cfg->cs_gpio,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = LCD_SPI_MODE,
        .trans_queue_depth = LCD_TRANS_QUEUE_DEPTH,
        .on_color_trans_done = color_trans_done_bridge_cb,
        .user_ctx = ctx,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)cfg->spi_host, &panel_io_cfg, &ctx->panel_io);
    if (ret != ESP_OK) {
        (void)st7789_close(ctx);
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num = cfg->reset_gpio,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .data_endian = cfg->data_endian,
    };
    ret = esp_lcd_new_panel_st7789(ctx->panel_io, &panel_dev_cfg, &ctx->panel);
    if (ret == ESP_OK) {
        ret = esp_lcd_panel_reset(ctx->panel);
    }
    if (ret == ESP_OK && cfg->select_cb != NULL) {
        ret = cfg->select_cb(cfg->select_ctx, true);
    }
    if (ret == ESP_OK) {
        ret = esp_lcd_panel_init(ctx->panel);
    }
    if (ret == ESP_OK && cfg->invert_color) {
        ret = esp_lcd_panel_invert_color(ctx->panel, true);
    }
    if (ret == ESP_OK) {
        ret = esp_lcd_panel_swap_xy(ctx->panel, cfg->swap_xy);
    }
    if (ret == ESP_OK) {
        ret = esp_lcd_panel_mirror(ctx->panel, cfg->mirror_x, cfg->mirror_y);
    }
    if (ret == ESP_OK) {
        ret = esp_lcd_panel_disp_on_off(ctx->panel, true);
    }
    if (ret != ESP_OK) {
        (void)st7789_close(ctx);
        return ret;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t st7789_close(st7789_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;
    if (handle->panel != NULL) {
        esp_err_t ret = esp_lcd_panel_del(handle->panel);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (handle->cfg->select_cb != NULL) {
        esp_err_t ret = handle->cfg->select_cb(handle->cfg->select_ctx, false);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (handle->panel_io != NULL) {
        esp_err_t ret = esp_lcd_panel_io_del(handle->panel_io);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (handle->spi_inited) {
        esp_err_t ret = spi_bus_free(handle->cfg->spi_host);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (handle->transfer_done_sem != NULL) {
        vSemaphoreDelete(handle->transfer_done_sem);
    }
    free(handle);
    return first_err;
}

esp_err_t st7789_write_pixels(st7789_handle_t handle,
                              uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height,
                              const void *pixels)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(pixels != NULL, ESP_ERR_INVALID_ARG, TAG, "pixels is null");
    ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "empty area");
    if (handle->transfer_done_sem != NULL) {
        (void)xSemaphoreTake(handle->transfer_done_sem, 0);
    }
    return esp_lcd_panel_draw_bitmap(handle->panel, x, y, x + width, y + height, pixels);
}

esp_err_t st7789_wait_transfer_done(st7789_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->transfer_done_sem != NULL, ESP_ERR_INVALID_STATE, TAG, "transfer semaphore missing");
    return xSemaphoreTake(handle->transfer_done_sem, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t st7789_set_transfer_done_cb(st7789_handle_t handle, esp_lcd_panel_io_color_trans_done_cb_t cb, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    handle->transfer_done_cb = cb;
    handle->transfer_done_user_ctx = user_ctx;
    return ESP_OK;
}

#include "bsp_display.h"

#include <stdlib.h>

#include "doers3_ioexp.h"
#include "doers3_pins.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "st7789.h"

#define TAG "bsp_display"

struct bsp_display_s {
    st7789_handle_t panel;
    uint16_t *fill_line_buf;
    bool ioexp_acquired;
};

static const bsp_display_desc_t s_desc = {
    .present = true,
    .has_backlight = true,
    .has_touch = true,
    .width = DOERS3_LCD_WIDTH,
    .height = DOERS3_LCD_HEIGHT,
    .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
};

static esp_err_t lcd_select_cb(void *user_ctx, bool selected)
{
    (void)user_ctx;
    return doers3_ioexp_write_pin(DOERS3_IOEXP_LCD_CS, !selected);
}

const bsp_display_desc_t *bsp_display_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    esp_err_t ret = ESP_OK;

    struct bsp_display_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    ret = doers3_ioexp_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->ioexp_acquired = true;

    static const st7789_config_t cfg = {
        .width = DOERS3_LCD_WIDTH,
        .height = DOERS3_LCD_HEIGHT,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .invert_color = true,
        .spi_host = DOERS3_LCD_SPI_HOST,
        .cs_gpio = GPIO_NUM_NC,
        .dc_gpio = DOERS3_LCD_DC,
        .mosi_gpio = DOERS3_LCD_MOSI,
        .sclk_gpio = DOERS3_LCD_SCLK,
        .reset_gpio = DOERS3_LCD_RESET,
        .swap_xy = true,
        .mirror_x = true,
        .mirror_y = false,
        .select_cb = lcd_select_cb,
    };
    ret = st7789_open(&cfg, &handle->panel);
    if (ret != ESP_OK) {
        goto err;
    }

    size_t fill_bytes = DOERS3_LCD_WIDTH * sizeof(uint16_t);
    handle->fill_line_buf = heap_caps_malloc(fill_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (handle->fill_line_buf == NULL) {
        handle->fill_line_buf = heap_caps_malloc(fill_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    ESP_GOTO_ON_FALSE(handle->fill_line_buf != NULL, ESP_ERR_NO_MEM, err, TAG, "fill buf alloc failed");

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)bsp_display_close(handle);
    }
    return ret;
}

esp_err_t bsp_display_close(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;
    if (handle->fill_line_buf != NULL) {
        heap_caps_free(handle->fill_line_buf);
    }
    if (handle->panel != NULL) {
        first_err = st7789_close(handle->panel);
    }
    if (handle->ioexp_acquired) {
        esp_err_t ret = doers3_ioexp_release();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    free(handle);
    return first_err;
}

esp_err_t bsp_display_draw_bitmap(bsp_display_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return st7789_draw_bitmap(handle->panel, x_start, y_start, x_end, y_end, color_data);
}

esp_err_t bsp_display_fill(bsp_display_handle_t handle, uint16_t color)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->fill_line_buf != NULL, ESP_ERR_INVALID_STATE, TAG, "fill buffer missing");
    for (uint16_t x = 0; x < DOERS3_LCD_WIDTH; ++x) {
        handle->fill_line_buf[x] = color;
    }
    for (uint16_t y = 0; y < DOERS3_LCD_HEIGHT; ++y) {
        ESP_RETURN_ON_ERROR(bsp_display_draw_bitmap(handle, 0, y, DOERS3_LCD_WIDTH, y + 1, handle->fill_line_buf),
                            TAG,
                            "fill draw failed");
    }
    return st7789_flush_wait(handle->panel);
}

esp_err_t bsp_display_register_flush_done_cb(bsp_display_handle_t handle, esp_lcd_panel_io_color_trans_done_cb_t cb, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return st7789_register_flush_done_cb(handle->panel, cb, user_ctx);
}

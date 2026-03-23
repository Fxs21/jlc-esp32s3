#include "bsp_display.h"
#include "bsp_display_internal.h"

#include <stdlib.h>

#include "bsp_lvgl_port.h"
#include "doers3_ioexp.h"
#include "doers3_pins.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"
#include "st7789.h"

#define TAG "bsp_display"
#define DISPLAY_BITS_PER_PIXEL 16

struct bsp_display_s {
    st7789_handle_t panel;
    bsp_lvgl_port_t port;
    bool ioexp_acquired;
};

static const bsp_display_info_t s_info = {
    .present = true,
    .width = DOERS3_LCD_WIDTH,
    .height = DOERS3_LCD_HEIGHT,
    .bits_per_pixel = DISPLAY_BITS_PER_PIXEL,
    .align_x = 1,
    .align_y = 1,
    .max_transfer_bytes = DOERS3_LCD_WIDTH * DOERS3_LCD_HEIGHT * sizeof(uint16_t),
};

static bool s_display_open;

static esp_err_t lcd_select_cb(void *user_ctx, bool selected)
{
    (void)user_ctx;
    return doers3_ioexp_set_lcd_cs(selected);
}

static bool lvgl_flush_ready_cb(void *user_ctx)
{
    lv_display_t *display = (lv_display_t *)user_ctx;
    lv_display_flush_ready(display);
    return false;
}

static size_t required_data_size(uint16_t width, uint16_t height)
{
    return (size_t)width * height * DISPLAY_BITS_PER_PIXEL / 8;
}

static esp_err_t validate_write_args(bsp_display_handle_t handle,
                                     uint16_t x,
                                     uint16_t y,
                                     uint16_t width,
                                     uint16_t height,
                                     const void *data,
                                     size_t data_size)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is null");
    ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "empty area");
    ESP_RETURN_ON_FALSE(x < DOERS3_LCD_WIDTH && y < DOERS3_LCD_HEIGHT, ESP_ERR_INVALID_ARG, TAG, "origin out of range");
    ESP_RETURN_ON_FALSE((uint32_t)x + width <= DOERS3_LCD_WIDTH && (uint32_t)y + height <= DOERS3_LCD_HEIGHT,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "area out of range");
    size_t required = required_data_size(width, height);
    if (data_size < required) {
        ESP_LOGW(TAG, "display data too small: need=%u got=%u", (unsigned)required, (unsigned)data_size);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    struct bsp_display_s *handle = (struct bsp_display_s *)lv_display_get_user_data(display);
    if (handle == NULL) {
        lv_display_flush_ready(display);
        return;
    }

    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    // DoerS3 ST7789 is configured little-endian, matching LVGL's RGB565 buffer layout.
    esp_err_t ret = bsp_display_write_async(handle,
                                            area->x1,
                                            area->y1,
                                            width,
                                            height,
                                            px_map,
                                            required_data_size(width, height));
    if (ret != ESP_OK) {
        lv_display_flush_ready(display);
    }
}

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(!s_display_open, ESP_ERR_INVALID_STATE, TAG, "display already open");
    *out_handle = NULL;
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
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
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

    s_display_open = true;
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
    ESP_RETURN_ON_FALSE(handle->port.lv_display == NULL, ESP_ERR_INVALID_STATE, TAG, "LVGL port still open");
    esp_err_t first_err = ESP_OK;
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
    s_display_open = false;
    return first_err;
}

const bsp_display_info_t *bsp_display_get_info(bsp_display_handle_t handle)
{
    return handle == NULL ? NULL : &s_info;
}

esp_err_t bsp_display_set_transfer_done_cb(bsp_display_handle_t handle,
                                           bsp_display_transfer_done_cb_t cb,
                                           void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    handle->port.transfer_done_cb = cb;
    handle->port.transfer_done_user_ctx = user_ctx;
    return st7789_set_transfer_done_cb(handle->panel,
                                       cb == NULL ? NULL : bsp_lvgl_transfer_done_bridge,
                                       &handle->port);
}

esp_err_t bsp_display_write_async(bsp_display_handle_t handle,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  const void *data,
                                  size_t data_size)
{
    ESP_RETURN_ON_ERROR(validate_write_args(handle, x, y, width, height, data, data_size), TAG, "invalid write args");
    return st7789_write_pixels(handle->panel, x, y, width, height, data);
}

esp_err_t bsp_display_wait_transfer_done(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return st7789_wait_transfer_done(handle->panel);
}

esp_err_t bsp_display_port_lvgl_open(bsp_display_handle_t handle, lv_display_t **out_display)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_display != NULL, ESP_ERR_INVALID_ARG, TAG, "out_display is null");
    *out_display = NULL;

    ESP_RETURN_ON_ERROR(bsp_lvgl_port_open(&handle->port, DOERS3_LCD_WIDTH, DOERS3_LCD_HEIGHT, handle),
                        TAG, "lvgl port open failed");
    lv_display_set_flush_cb(handle->port.lv_display, lvgl_flush_cb);
    ESP_RETURN_ON_ERROR(bsp_display_set_transfer_done_cb(handle, lvgl_flush_ready_cb, handle->port.lv_display),
                        TAG, "set transfer callback failed");

    *out_display = handle->port.lv_display;
    return ESP_OK;
}

esp_err_t bsp_display_port_lvgl_close(bsp_display_handle_t handle, lv_display_t *display)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");

    (void)bsp_display_set_transfer_done_cb(handle, NULL, NULL);
    return bsp_lvgl_port_close(&handle->port, display);
}

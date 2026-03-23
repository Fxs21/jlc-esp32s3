#include "bsp_display.h"
#include "bsp_display_internal.h"

#include <stdlib.h>

#include "auras3_panel.h"
#include "auras3_pins.h"
#include "bsp_lvgl_port.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

#define TAG "bsp_display"
#define DISPLAY_BITS_PER_PIXEL 16
#define DISPLAY_ALIGN_X 2
#define DISPLAY_ALIGN_Y 2

struct bsp_display_s {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    bsp_lvgl_port_t port;
    bool lv_current_flush_is_last;
};

static inline size_t display_required_data_size(uint16_t width, uint16_t height)
{
    return (size_t)width * height * DISPLAY_BITS_PER_PIXEL / 8;
}

static const bsp_display_info_t s_info = {
    .present = true,
    .width = AURAS3_LCD_WIDTH,
    .height = AURAS3_LCD_HEIGHT,
    .bits_per_pixel = DISPLAY_BITS_PER_PIXEL,
    .align_x = DISPLAY_ALIGN_X,
    .align_y = DISPLAY_ALIGN_Y,
    .max_transfer_bytes = AURAS3_LCD_WIDTH * AURAS3_LCD_HEIGHT * sizeof(uint16_t),
};

static bool s_display_open;

// ---------- LVGL callbacks ----------

static bool lvgl_flush_ready_cb(void *user_ctx)
{
    struct bsp_display_s *handle = (struct bsp_display_s *)user_ctx;
    if (handle == NULL || handle->port.lv_display == NULL) {
        return false;
    }
    lv_display_flush_ready(handle->port.lv_display);
    if (handle->lv_current_flush_is_last) {
        handle->lv_current_flush_is_last = false;
    }
    return false;
}

static void lvgl_log_startup_info(const struct bsp_display_s *handle)
{
    multi_heap_info_t internal = {0};
    multi_heap_info_t dma = {0};
    multi_heap_info_t psram = {0};
    heap_caps_get_info(&internal, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    heap_caps_get_info(&dma, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    heap_caps_get_info(&psram, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG,
             "screen: panel=CO5300 %ux%u RGB565 QSPI gap=%u,%u align=%ux%u lvgl=PARTIAL swap=on te_gpio=%d te_wait=off",
             AURAS3_LCD_WIDTH,
             AURAS3_LCD_HEIGHT,
             AURAS3_LCD_X_GAP,
             AURAS3_LCD_Y_GAP,
             DISPLAY_ALIGN_X,
             DISPLAY_ALIGN_Y,
             AURAS3_LCD_TE);
    ESP_LOGI(TAG,
             "memory: lvgl_buf=%s double lines=%u one=%u total=%u heap_free internal=%u dma=%u psram=%u largest internal=%u dma=%u psram=%u",
             handle->port.lv_buffer.memory,
             (unsigned)handle->port.lv_buffer.lines,
             (unsigned)handle->port.lv_buffer.size,
             (unsigned)(handle->port.lv_buffer.size * 2),
             (unsigned)internal.total_free_bytes,
             (unsigned)dma.total_free_bytes,
             (unsigned)psram.total_free_bytes,
             (unsigned)internal.largest_free_block,
             (unsigned)dma.largest_free_block,
             (unsigned)psram.largest_free_block);
}

#if LVGL_VERSION_MAJOR >= 9
static void rounder_event_cb(lv_event_t *event)
{
    lv_area_t *area = (lv_area_t *)lv_event_get_param(event);
    if (area == NULL) {
        return;
    }
    // CO5300 QSPI writes are restricted to the panel's 2-pixel granularity.
    area->x1 = (area->x1 / DISPLAY_ALIGN_X) * DISPLAY_ALIGN_X;
    area->y1 = (area->y1 / DISPLAY_ALIGN_Y) * DISPLAY_ALIGN_Y;
    area->x2 = ((area->x2 + DISPLAY_ALIGN_X) / DISPLAY_ALIGN_X) * DISPLAY_ALIGN_X - 1;
    area->y2 = ((area->y2 + DISPLAY_ALIGN_Y) / DISPLAY_ALIGN_Y) * DISPLAY_ALIGN_Y - 1;
    if (area->x1 < 0) {
        area->x1 = 0;
    }
    if (area->y1 < 0) {
        area->y1 = 0;
    }
    if (area->x2 >= AURAS3_LCD_WIDTH) {
        area->x2 = AURAS3_LCD_WIDTH - 1;
    }
    if (area->y2 >= AURAS3_LCD_HEIGHT) {
        area->y2 = AURAS3_LCD_HEIGHT - 1;
    }
}
#endif

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    struct bsp_display_s *handle = (struct bsp_display_s *)lv_display_get_user_data(display);
    if (handle == NULL) {
        lv_display_flush_ready(display);
        return;
    }

    const uint16_t width = area->x2 - area->x1 + 1;
    const uint16_t height = area->y2 - area->y1 + 1;
    const size_t tx_size = display_required_data_size(width, height);

    handle->lv_current_flush_is_last = lv_display_flush_is_last(display);
    // AuraS3 native transfers are high-byte-first RGB565; LVGL buffers are host-endian RGB565.
    lv_draw_sw_rgb565_swap(px_map, (uint32_t)width * height);
    esp_err_t ret = bsp_display_write_async(handle, area->x1, area->y1, width, height, px_map, tx_size);

    if (ret != ESP_OK) {
        if (handle->lv_current_flush_is_last) {
            handle->lv_current_flush_is_last = false;
        }
        lv_display_flush_ready(display);
        ESP_LOGW(TAG, "LVGL flush failed: area=%d,%d %ux%u err=%s", area->x1, area->y1, width, height, esp_err_to_name(ret));
    }
}

// ---------- public display API ----------

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
    ESP_RETURN_ON_FALSE(x < AURAS3_LCD_WIDTH && y < AURAS3_LCD_HEIGHT, ESP_ERR_INVALID_ARG, TAG, "origin out of range");
    ESP_RETURN_ON_FALSE((uint32_t)x + width <= AURAS3_LCD_WIDTH && (uint32_t)y + height <= AURAS3_LCD_HEIGHT,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "area out of range");
    if ((x % DISPLAY_ALIGN_X) != 0 || (y % DISPLAY_ALIGN_Y) != 0 ||
        (width % DISPLAY_ALIGN_X) != 0 || (height % DISPLAY_ALIGN_Y) != 0) {
        ESP_LOGW(TAG,
                 "display area is not aligned: x=%u y=%u w=%u h=%u align=%ux%u",
                 (unsigned)x,
                 (unsigned)y,
                 (unsigned)width,
                 (unsigned)height,
                 DISPLAY_ALIGN_X,
                 DISPLAY_ALIGN_Y);
        return ESP_ERR_INVALID_ARG;
    }
    size_t required = display_required_data_size(width, height);
    if (data_size < required) {
        ESP_LOGW(TAG, "display data too small: need=%u got=%u", (unsigned)required, (unsigned)data_size);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t write_native_once(bsp_display_handle_t handle,
                                   uint16_t x,
                                   uint16_t y,
                                   uint16_t width,
                                   uint16_t height,
                                   const void *data)
{
    ESP_RETURN_ON_ERROR(auras3_panel_prepare_transfer(), TAG, "prepare transfer failed");
    return esp_lcd_panel_draw_bitmap(handle->panel, x, y, x + width, y + height, data);
}

static esp_err_t restore_transfer_done_cb(bsp_display_handle_t handle)
{
    esp_lcd_panel_io_color_trans_done_cb_t cb = handle->port.transfer_done_cb == NULL
                                                ? NULL : bsp_lvgl_transfer_done_bridge;
    return auras3_panel_set_transfer_done_cb(cb, &handle->port);
}

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(!s_display_open, ESP_ERR_INVALID_STATE, TAG, "display already open");
    *out_handle = NULL;

    struct bsp_display_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    esp_err_t ret = auras3_panel_acquire(&handle->panel, &handle->io);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    s_display_open = true;
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_display_close(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->port.lv_display == NULL, ESP_ERR_INVALID_STATE, TAG, "LVGL port still open");
    (void)auras3_panel_set_transfer_done_cb(NULL, NULL);
    esp_err_t ret = auras3_panel_release();
    free(handle);
    s_display_open = false;
    return ret;
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
    return restore_transfer_done_cb(handle);
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
    return write_native_once(handle, x, y, width, height, data);
}

esp_err_t bsp_display_wait_transfer_done(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return auras3_panel_wait_transfer_done();
}

// ---------- internal LVGL port API ----------

esp_err_t bsp_display_port_lvgl_open(bsp_display_handle_t handle, lv_display_t **out_display)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_display != NULL, ESP_ERR_INVALID_ARG, TAG, "out_display is null");
    *out_display = NULL;

    ESP_RETURN_ON_ERROR(bsp_lvgl_port_open(&handle->port, AURAS3_LCD_WIDTH, AURAS3_LCD_HEIGHT, handle),
                        TAG, "lvgl port open failed");
    lvgl_log_startup_info(handle);
    lv_display_set_flush_cb(handle->port.lv_display, lvgl_flush_cb);
#if LVGL_VERSION_MAJOR >= 9
    lv_display_add_event_cb(handle->port.lv_display, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
#endif
    ESP_RETURN_ON_ERROR(bsp_display_set_transfer_done_cb(handle, lvgl_flush_ready_cb, handle),
                        TAG, "set transfer callback failed");

    *out_display = handle->port.lv_display;
    return ESP_OK;
}

esp_err_t bsp_display_port_lvgl_close(bsp_display_handle_t handle, lv_display_t *display)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");

    (void)bsp_display_set_transfer_done_cb(handle, NULL, NULL);
    esp_err_t ret = bsp_lvgl_port_close(&handle->port, display);
    handle->lv_current_flush_is_last = false;
    return ret;
}

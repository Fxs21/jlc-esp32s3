#include <stdbool.h>
#include <stdlib.h>

#include "bsp_ui.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#define BSP_UI_TAG "bsp_ui"

struct bsp_ui_s {
    bsp_display_handle_t display;
    bsp_touch_handle_t touch;
    bsp_backlight_handle_t backlight;
    lv_display_t *lv_display;
    lv_indev_t *lv_indev;
    lv_color16_t *buf1;
    lv_color16_t *buf2;
    uint32_t buf_size;
};

static uint32_t lv_tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool on_flush_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    lv_display_t *display = (lv_display_t *)user_ctx;
    lv_display_flush_ready(display);
    return false;
}

static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    bsp_display_handle_t disp = (bsp_display_handle_t)lv_display_get_user_data(display);
    if (disp == NULL) {
        lv_display_flush_ready(display);
        return;
    }

    esp_err_t ret = bsp_display_draw_bitmap(disp, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    if (ret != ESP_OK) {
        ESP_LOGE(BSP_UI_TAG, "display flush failed: %s", esp_err_to_name(ret));
        lv_display_flush_ready(display);
    }
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    bsp_touch_handle_t touch = (bsp_touch_handle_t)lv_indev_get_user_data(indev);
    if (touch == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_lcd_touch_point_data_t points[1];
    size_t points_num = 0;
    esp_err_t ret = bsp_touch_read(touch, points, 1, &points_num);
    if (ret != ESP_OK || points_num == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->point.x = points[0].x;
    data->point.y = points[0].y;
    data->state = LV_INDEV_STATE_PRESSED;
}

esp_err_t bsp_ui_open(bsp_ui_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_UI_TAG, "out_handle is null");
    esp_err_t ret = ESP_OK;

    struct bsp_ui_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_UI_TAG, "no memory");

    if (!lv_is_initialized()) {
        lv_init();
        lv_tick_set_cb(lv_tick_cb);
    }

    const bsp_display_desc_t *screen_desc = bsp_display_get_desc();
    ESP_GOTO_ON_FALSE(screen_desc != NULL && screen_desc->present,
                      ESP_ERR_NOT_SUPPORTED, err, BSP_UI_TAG, "screen not present");

    ESP_GOTO_ON_ERROR(bsp_display_open(&handle->display), err, BSP_UI_TAG, "display open failed");

    handle->lv_display = lv_display_create(screen_desc->width, screen_desc->height);
    ESP_GOTO_ON_FALSE(handle->lv_display != NULL, ESP_ERR_NO_MEM, err, BSP_UI_TAG, "create lv display failed");

    lv_color_format_t lv_color_format = LV_COLOR_FORMAT_RGB565;
    if (screen_desc->data_endian == LCD_RGB_DATA_ENDIAN_BIG && LV_DRAW_SW_SUPPORT_RGB565_SWAPPED) {
        lv_color_format = LV_COLOR_FORMAT_RGB565_SWAPPED;
    }
    lv_display_set_color_format(handle->lv_display, lv_color_format);
    lv_display_set_user_data(handle->lv_display, handle->display);

    handle->buf_size = screen_desc->width * screen_desc->height * sizeof(lv_color16_t);
    handle->buf1 = heap_caps_malloc(handle->buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    handle->buf2 = heap_caps_malloc(handle->buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (handle->buf1 == NULL || handle->buf2 == NULL) {
        ret = ESP_ERR_NO_MEM;
        ESP_LOGE(BSP_UI_TAG, "alloc lvgl buffers failed");
        goto err;
    }

    lv_display_set_buffers(handle->lv_display,
                           handle->buf1,
                           handle->buf2,
                           handle->buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(handle->lv_display, flush_cb);

    ESP_GOTO_ON_ERROR(bsp_display_register_flush_done_cb(handle->display, on_flush_done, handle->lv_display),
                      err, BSP_UI_TAG, "register flush done cb failed");

    if (bsp_touch_get_desc()->present) {
        ESP_GOTO_ON_ERROR(bsp_touch_open(&handle->touch), err, BSP_UI_TAG, "touch open failed");
        handle->lv_indev = lv_indev_create();
        ESP_GOTO_ON_FALSE(handle->lv_indev != NULL, ESP_ERR_NO_MEM, err, BSP_UI_TAG, "create indev failed");
        lv_indev_set_type(handle->lv_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_user_data(handle->lv_indev, handle->touch);
        lv_indev_set_read_cb(handle->lv_indev, touch_read_cb);
    }

    if (bsp_backlight_get_desc()->present) {
        ESP_GOTO_ON_ERROR(bsp_backlight_open(&handle->backlight),
                          err,
                          BSP_UI_TAG,
                          "backlight open failed");
    }

    ESP_LOGI(BSP_UI_TAG,
             "display=%ux%u touch=%d backlight=%d buffers=%lu",
             screen_desc->width,
             screen_desc->height,
             handle->touch != NULL,
             handle->backlight != NULL,
             (unsigned long)(handle->buf_size * 2));

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)bsp_ui_close(handle);
    }
    return ret;
}

esp_err_t bsp_ui_process(bsp_ui_handle_t handle, uint32_t *out_delay_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_UI_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_delay_ms != NULL, ESP_ERR_INVALID_ARG, BSP_UI_TAG, "out_delay_ms is null");

    *out_delay_ms = lv_timer_handler();
    return ESP_OK;
}

lv_display_t *bsp_ui_get_display(bsp_ui_handle_t handle)
{
    return handle == NULL ? NULL : handle->lv_display;
}

lv_indev_t *bsp_ui_get_indev(bsp_ui_handle_t handle)
{
    return handle == NULL ? NULL : handle->lv_indev;
}

esp_err_t bsp_ui_fill(bsp_ui_handle_t handle, uint16_t color)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_UI_TAG, "handle is null");
    return bsp_display_fill(handle->display, color);
}

esp_err_t bsp_ui_set_backlight(bsp_ui_handle_t handle, uint8_t percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_UI_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->backlight != NULL, ESP_ERR_NOT_SUPPORTED, BSP_UI_TAG, "backlight not present");
    return bsp_backlight_set_percent(handle->backlight, percent);
}

esp_err_t bsp_ui_close(bsp_ui_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_UI_TAG, "handle is null");

    if (handle->lv_indev != NULL) {
        lv_indev_delete(handle->lv_indev);
    }
    if (handle->lv_display != NULL) {
        lv_display_delete(handle->lv_display);
    }
    if (handle->touch != NULL) {
        (void)bsp_touch_close(handle->touch);
    }
    if (handle->backlight != NULL) {
        (void)bsp_backlight_close(handle->backlight);
    }
    if (handle->display != NULL) {
        (void)bsp_display_close(handle->display);
    }
    if (handle->buf1 != NULL) {
        heap_caps_free(handle->buf1);
    }
    if (handle->buf2 != NULL) {
        heap_caps_free(handle->buf2);
    }
    free(handle);
    return ESP_OK;
}

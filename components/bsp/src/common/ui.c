#include <stdlib.h>

#include "bsp_display.h"
#include "bsp_display_internal.h"
#include "bsp_ui.h"
#include "esp_check.h"
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
};

static uint32_t lv_tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    bsp_touch_handle_t touch = (bsp_touch_handle_t)lv_indev_get_user_data(indev);
    if (touch == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    bsp_touch_point_t points[1];
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
    *out_handle = NULL;
    esp_err_t ret = ESP_OK;

    struct bsp_ui_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_UI_TAG, "no memory");

    if (!lv_is_initialized()) {
        lv_init();
        lv_tick_set_cb(lv_tick_cb);
    }

    // UI owns the native display handle, so bsp_display_open() and bsp_ui_open() stay mutually exclusive.
    ESP_GOTO_ON_ERROR(bsp_display_open(&handle->display), err, BSP_UI_TAG, "display open failed");
    const bsp_display_info_t *screen_info = bsp_display_get_info(handle->display);
    ESP_GOTO_ON_FALSE(screen_info != NULL && screen_info->present,
                      ESP_ERR_NOT_SUPPORTED, err, BSP_UI_TAG, "screen not present");
    ESP_GOTO_ON_ERROR(bsp_display_port_lvgl_open(handle->display, &handle->lv_display),
                      err,
                      BSP_UI_TAG,
                      "display LVGL port open failed");

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

lv_display_t *bsp_ui_get_lvgl_display(bsp_ui_handle_t handle)
{
    return handle == NULL ? NULL : handle->lv_display;
}

lv_indev_t *bsp_ui_get_lvgl_indev(bsp_ui_handle_t handle)
{
    return handle == NULL ? NULL : handle->lv_indev;
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
        (void)bsp_display_port_lvgl_close(handle->display, handle->lv_display);
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
    free(handle);
    return ESP_OK;
}

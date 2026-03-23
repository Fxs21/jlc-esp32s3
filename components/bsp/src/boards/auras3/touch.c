#include "bsp_touch.h"

#include "esp_check.h"

#define TAG "bsp_touch"

static const bsp_touch_desc_t s_desc = {
    .present = false,
};

const bsp_touch_desc_t *bsp_touch_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_touch_open(bsp_touch_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_touch_close(bsp_touch_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

esp_err_t bsp_touch_read(bsp_touch_handle_t handle, esp_lcd_touch_point_data_t *points, size_t points_capacity, size_t *points_num)
{
    (void)points;
    (void)points_capacity;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(points_num != NULL, ESP_ERR_INVALID_ARG, TAG, "points_num is null");
    *points_num = 0;
    return ESP_ERR_NOT_SUPPORTED;
}

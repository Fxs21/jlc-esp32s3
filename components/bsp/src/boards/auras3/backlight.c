#include "bsp_backlight.h"

#include "esp_check.h"

#define TAG "bsp_backlight"

static const bsp_backlight_desc_t s_desc = {
    .present = false,
};

const bsp_backlight_desc_t *bsp_backlight_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_backlight_open(bsp_backlight_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_backlight_close(bsp_backlight_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

esp_err_t bsp_backlight_set_percent(bsp_backlight_handle_t handle, uint8_t percent)
{
    (void)percent;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_backlight_get_percent(bsp_backlight_handle_t handle, uint8_t *out_percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_percent != NULL, ESP_ERR_INVALID_ARG, TAG, "out_percent is null");
    *out_percent = 0;
    return ESP_ERR_NOT_SUPPORTED;
}

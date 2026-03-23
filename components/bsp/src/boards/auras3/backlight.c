#include "bsp_backlight.h"

#include <stdlib.h>

#include "auras3_panel.h"
#include "esp_check.h"

#define TAG "bsp_backlight"

struct bsp_backlight_s {
    uint8_t percent;
};

static const bsp_backlight_desc_t s_desc = {
    .present = true,
};

const bsp_backlight_desc_t *bsp_backlight_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_backlight_open(bsp_backlight_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;

    struct bsp_backlight_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    esp_err_t ret = auras3_panel_acquire(NULL, NULL);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }
    ret = auras3_panel_get_brightness(&handle->percent);
    if (ret != ESP_OK) {
        (void)auras3_panel_release();
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_backlight_close(bsp_backlight_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t ret = auras3_panel_release();
    free(handle);
    return ret;
}

esp_err_t bsp_backlight_set_percent(bsp_backlight_handle_t handle, uint8_t percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    if (percent > 100) {
        percent = 100;
    }
    ESP_RETURN_ON_ERROR(auras3_panel_set_brightness(percent), TAG, "set brightness failed");
    handle->percent = percent;
    return ESP_OK;
}

esp_err_t bsp_backlight_get_percent(bsp_backlight_handle_t handle, uint8_t *out_percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_percent != NULL, ESP_ERR_INVALID_ARG, TAG, "out_percent is null");
    *out_percent = handle->percent;
    return ESP_OK;
}

#include "bsp_backlight.h"

#include <stdlib.h>

#include "doers3_pins.h"
#include "esp_check.h"
#include "backlight_ledc.h"

#define TAG "bsp_backlight"

struct bsp_backlight_s {
    backlight_ledc_handle_t driver;
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

    struct bsp_backlight_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    const backlight_ledc_config_t cfg = {
        .gpio = DOERS3_BACKLIGHT_GPIO,
        .output_invert = DOERS3_BACKLIGHT_OUTPUT_INVERT,
    };
    esp_err_t ret = backlight_ledc_open(&cfg, &handle->driver);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_backlight_close(bsp_backlight_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t ret = ESP_OK;
    if (handle->driver != NULL) {
        ret = backlight_ledc_close(handle->driver);
    }
    free(handle);
    return ret;
}

esp_err_t bsp_backlight_set_percent(bsp_backlight_handle_t handle, uint8_t percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_ERROR(backlight_ledc_set_percent(handle->driver, percent), TAG, "set percent failed");
    handle->percent = percent > 100 ? 100 : percent;
    return ESP_OK;
}

esp_err_t bsp_backlight_get_percent(bsp_backlight_handle_t handle, uint8_t *out_percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_percent != NULL, ESP_ERR_INVALID_ARG, TAG, "out_percent is null");
    *out_percent = handle->percent;
    return ESP_OK;
}

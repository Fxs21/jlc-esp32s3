#include <stdlib.h>

#include "bsp_backlight.h"
#include "bsp_backlight_backend.h"
#include "esp_check.h"

#define BSP_BACKLIGHT_TAG "bsp_backlight"

struct bsp_backlight_s {
    const bsp_backlight_backend_t *backend;
    void *ctx;
    uint8_t percent;
};

static bsp_backlight_desc_t backlight_desc;

const bsp_backlight_desc_t *bsp_backlight_get_desc(void)
{
    backlight_desc.present = (bsp_backlight_backend_get() != NULL);
    return &backlight_desc;
}

esp_err_t bsp_backlight_open(bsp_backlight_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_BACKLIGHT_TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(bsp_backlight_get_desc()->present,
                        ESP_ERR_NOT_SUPPORTED,
                        BSP_BACKLIGHT_TAG,
                        "backlight not present");

    struct bsp_backlight_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_BACKLIGHT_TAG, "no memory");

    handle->backend = bsp_backlight_backend_get();
    ESP_RETURN_ON_FALSE(handle->backend != NULL,
                        ESP_ERR_NOT_SUPPORTED,
                        BSP_BACKLIGHT_TAG,
                        "backlight backend not configured");

    esp_err_t ret = handle->backend->create(handle->backend->config, &handle->ctx);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_backlight_close(bsp_backlight_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_BACKLIGHT_TAG, "handle is null");
    if (handle->backend != NULL && handle->ctx != NULL) {
        (void)handle->backend->destroy(handle->ctx);
    }
    free(handle);
    return ESP_OK;
}

esp_err_t bsp_backlight_set_percent(bsp_backlight_handle_t handle, uint8_t percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_BACKLIGHT_TAG, "handle is null");
    ESP_RETURN_ON_ERROR(handle->backend->set_percent(handle->ctx, percent),
                        BSP_BACKLIGHT_TAG,
                        "set backlight failed");
    handle->percent = percent > 100 ? 100 : percent;
    return ESP_OK;
}

esp_err_t bsp_backlight_get_percent(bsp_backlight_handle_t handle, uint8_t *out_percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_BACKLIGHT_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_percent != NULL, ESP_ERR_INVALID_ARG, BSP_BACKLIGHT_TAG, "out_percent is null");
    *out_percent = handle->percent;
    return ESP_OK;
}

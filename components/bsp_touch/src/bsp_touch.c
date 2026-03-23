#include <stdlib.h>

#include "bsp_touch.h"
#include "bsp_touch_backend.h"
#include "esp_check.h"

#define BSP_TOUCH_TAG "bsp_touch"

struct bsp_touch_s {
    const bsp_touch_backend_t *backend;
    void *ctx;
};

static bsp_touch_desc_t touch_desc;

const bsp_touch_desc_t *bsp_touch_get_desc(void)
{
    touch_desc.present = (bsp_touch_backend_get() != NULL);
    return &touch_desc;
}

esp_err_t bsp_touch_open(bsp_touch_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_TOUCH_TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(bsp_touch_get_desc()->present, ESP_ERR_NOT_SUPPORTED, BSP_TOUCH_TAG, "touch not present");

    struct bsp_touch_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_TOUCH_TAG, "no memory");

    handle->backend = bsp_touch_backend_get();
    ESP_RETURN_ON_FALSE(handle->backend != NULL, ESP_ERR_NOT_SUPPORTED, BSP_TOUCH_TAG, "touch backend not configured");

    esp_err_t ret = handle->backend->create(handle->backend->config, &handle->ctx);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_touch_close(bsp_touch_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_TOUCH_TAG, "handle is null");
    if (handle->backend != NULL && handle->ctx != NULL) {
        (void)handle->backend->destroy(handle->ctx);
    }
    free(handle);
    return ESP_OK;
}

esp_err_t bsp_touch_read(bsp_touch_handle_t handle,
                         esp_lcd_touch_point_data_t *points,
                         size_t points_capacity,
                         size_t *points_num)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_TOUCH_TAG, "handle is null");
    return handle->backend->read(handle->ctx, points, points_capacity, points_num);
}

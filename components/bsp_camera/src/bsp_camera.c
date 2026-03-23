#include <stdlib.h>

#include "bsp_camera.h"
#include "bsp_camera_backend.h"
#include "esp_check.h"

#define BSP_CAMERA_TAG "bsp_camera"

struct bsp_camera_s {
    const bsp_camera_backend_t *backend;
    void *ctx;
};

bool bsp_camera_present(void)
{
    const bsp_camera_backend_t *backend = bsp_camera_backend_get();
    return backend != NULL;
}

esp_err_t bsp_camera_new(bsp_camera_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "out_handle is null");

    const bsp_camera_backend_t *backend = bsp_camera_backend_get();
    ESP_RETURN_ON_FALSE(backend != NULL, ESP_ERR_NOT_SUPPORTED, BSP_CAMERA_TAG, "camera backend not found");

    struct bsp_camera_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_CAMERA_TAG, "no memory");
    handle->backend = backend;

    esp_err_t ret = backend->create(backend->config, &handle->ctx);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_camera_start(bsp_camera_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->backend != NULL && handle->backend->start != NULL,
                        ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "camera backend not initialized");
    return handle->backend->start(handle->ctx);
}

esp_err_t bsp_camera_fb_get(bsp_camera_handle_t handle, camera_fb_t **out_frame)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->backend != NULL && handle->backend->fb_get != NULL,
                        ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "camera backend not initialized");
    return handle->backend->fb_get(handle->ctx, out_frame);
}

esp_err_t bsp_camera_fb_return(bsp_camera_handle_t handle, camera_fb_t *frame)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->backend != NULL && handle->backend->fb_return != NULL,
                        ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "camera backend not initialized");
    return handle->backend->fb_return(handle->ctx, frame);
}

esp_err_t bsp_camera_stop(bsp_camera_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->backend != NULL && handle->backend->stop != NULL,
                        ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "camera backend not initialized");
    return handle->backend->stop(handle->ctx);
}

sensor_t *bsp_camera_sensor(bsp_camera_handle_t handle)
{
    if (handle == NULL || handle->backend == NULL || handle->backend->sensor == NULL) {
        return NULL;
    }
    return handle->backend->sensor(handle->ctx);
}

esp_err_t bsp_camera_del(bsp_camera_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "handle is null");

    esp_err_t first_err = ESP_OK;
    if (handle->backend != NULL && handle->backend->destroy != NULL && handle->ctx != NULL) {
        first_err = handle->backend->destroy(handle->ctx);
        handle->ctx = NULL;
    }

    free(handle);
    return first_err;
}

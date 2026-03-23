#include "bsp_camera.h"

#include "esp_check.h"

#define TAG "bsp_camera"

static const bsp_camera_desc_t s_desc = {
    .present = false,
};

const bsp_camera_desc_t *bsp_camera_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_camera_open(bsp_camera_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_camera_start(bsp_camera_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_camera_fb_get(bsp_camera_handle_t handle, camera_fb_t **out_frame)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_frame != NULL, ESP_ERR_INVALID_ARG, TAG, "out_frame is null");
    *out_frame = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_camera_fb_return(bsp_camera_handle_t handle, camera_fb_t *frame)
{
    (void)frame;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_camera_stop(bsp_camera_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

sensor_t *bsp_camera_sensor(bsp_camera_handle_t handle)
{
    (void)handle;
    return NULL;
}

esp_err_t bsp_camera_close(bsp_camera_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

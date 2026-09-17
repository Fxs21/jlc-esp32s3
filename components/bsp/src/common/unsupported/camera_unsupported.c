#include "bsp_camera.h"

static const bsp_camera_desc_t s_desc = {
    .present = false,
};

const bsp_camera_desc_t *bsp_camera_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_camera_open(bsp_camera_handle_t *handle_out)
{
    if (handle_out != NULL) {
        *handle_out = NULL;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_camera_close(bsp_camera_handle_t handle)
{
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_camera_capture(bsp_camera_handle_t handle, bsp_camera_frame_t *frame_out)
{
    (void)handle;
    (void)frame_out;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_camera_release_frame(bsp_camera_handle_t handle, const bsp_camera_frame_t *frame)
{
    (void)handle;
    (void)frame;
    return ESP_ERR_NOT_SUPPORTED;
}

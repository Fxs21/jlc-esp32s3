#include "bsp_imu.h"

#include "esp_check.h"

#define TAG "bsp_imu"

static const bsp_imu_desc_t s_desc = {
    .present = false,
};

const bsp_imu_desc_t *bsp_imu_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_imu_open(bsp_imu_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_imu_close(bsp_imu_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

esp_err_t bsp_imu_read(bsp_imu_handle_t handle, bsp_imu_data_t *out_data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_data != NULL, ESP_ERR_INVALID_ARG, TAG, "out_data is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_imu_is_data_ready(bsp_imu_handle_t handle, bool *out_ready)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_ready != NULL, ESP_ERR_INVALID_ARG, TAG, "out_ready is null");
    *out_ready = false;
    return ESP_ERR_NOT_SUPPORTED;
}

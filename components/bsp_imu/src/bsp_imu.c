#include <stdlib.h>

#include "bsp_imu.h"
#include "bsp_imu_backend.h"
#include "esp_check.h"

#define BSP_IMU_TAG "bsp_imu"

struct bsp_imu_s {
    const bsp_imu_backend_t *backend;
    void *ctx;
};

bool bsp_imu_present(void)
{
    const bsp_imu_backend_t *backend = bsp_imu_backend_get();
    return backend != NULL;
}

esp_err_t bsp_imu_new(bsp_imu_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "out_handle is null");

    const bsp_imu_backend_t *backend = bsp_imu_backend_get();
    ESP_RETURN_ON_FALSE(backend != NULL, ESP_ERR_NOT_SUPPORTED, BSP_IMU_TAG, "imu backend not found");

    struct bsp_imu_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_IMU_TAG, "no memory");
    handle->backend = backend;

    esp_err_t ret = backend->create(backend->config, &handle->ctx);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_imu_del(bsp_imu_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "handle is null");

    esp_err_t first_err = ESP_OK;
    if (handle->backend != NULL && handle->backend->destroy != NULL && handle->ctx != NULL) {
        first_err = handle->backend->destroy(handle->ctx);
        handle->ctx = NULL;
    }

    free(handle);
    return first_err;
}

esp_err_t bsp_imu_read(bsp_imu_handle_t handle, bsp_imu_data_t *out_data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->backend != NULL && handle->backend->read != NULL,
                        ESP_ERR_INVALID_STATE, BSP_IMU_TAG, "imu backend not initialized");
    return handle->backend->read(handle->ctx, out_data);
}

esp_err_t bsp_imu_is_data_ready(bsp_imu_handle_t handle, bool *out_ready)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->backend != NULL && handle->backend->is_data_ready != NULL,
                        ESP_ERR_INVALID_STATE, BSP_IMU_TAG, "imu backend not initialized");
    return handle->backend->is_data_ready(handle->ctx, out_ready);
}

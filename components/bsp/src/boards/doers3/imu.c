#include "bsp_imu.h"

#include <stdlib.h>

#include "doers3_i2c.h"
#include "doers3_pins.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "qmi8658.h"

#define TAG "bsp_imu"

struct bsp_imu_s {
    qmi8658_dev_t dev;
    bool i2c_acquired;
};

static const bsp_imu_desc_t s_desc = {
    .present = true,
};

const bsp_imu_desc_t *bsp_imu_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_imu_open(bsp_imu_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    esp_err_t ret = ESP_OK;

    struct bsp_imu_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    ret = doers3_i2c_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->i2c_acquired = true;

    i2c_master_bus_handle_t bus = NULL;
    ret = doers3_i2c_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = qmi8658_init(&handle->dev, bus, DOERS3_IMU_I2C_ADDR);
    if (ret != ESP_OK) {
        goto err;
    }
    qmi8658_set_accel_unit_mps2(&handle->dev, true);
    qmi8658_set_gyro_unit_rads(&handle->dev, true);

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)bsp_imu_close(handle);
    }
    return ret;
}

esp_err_t bsp_imu_close(bsp_imu_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;
    if (handle->dev.dev_handle != NULL) {
        first_err = i2c_master_bus_rm_device(handle->dev.dev_handle);
    }
    if (handle->i2c_acquired) {
        esp_err_t ret = doers3_i2c_release();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    free(handle);
    return first_err;
}

esp_err_t bsp_imu_read(bsp_imu_handle_t handle, bsp_imu_data_t *out_data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_data != NULL, ESP_ERR_INVALID_ARG, TAG, "out_data is null");

    qmi8658_data_t data = {0};
    ESP_RETURN_ON_ERROR(qmi8658_read_sensor_data(&handle->dev, &data), TAG, "read sensor failed");
    out_data->accel_x = data.accelX;
    out_data->accel_y = data.accelY;
    out_data->accel_z = data.accelZ;
    out_data->gyro_x = data.gyroX;
    out_data->gyro_y = data.gyroY;
    out_data->gyro_z = data.gyroZ;
    out_data->temperature = data.temperature;
    out_data->timestamp = data.timestamp;
    return ESP_OK;
}

esp_err_t bsp_imu_is_data_ready(bsp_imu_handle_t handle, bool *out_ready)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_ready != NULL, ESP_ERR_INVALID_ARG, TAG, "out_ready is null");
    return qmi8658_is_data_ready(&handle->dev, out_ready);
}

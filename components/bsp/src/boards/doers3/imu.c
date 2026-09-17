#include "bsp_imu.h"

#include <stdlib.h>

#include "doers3_pins.h"
#include "bsp_i2c.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "qmi8658.h"

#define TAG "bsp_imu"

struct bsp_imu_s {
    qmi8658_dev_t dev;
    bool i2c_acquired;
};

static bool s_imu_open;

static const bsp_imu_desc_t s_desc = {
    .present = true,
};

const bsp_imu_desc_t *bsp_imu_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_imu_open(bsp_imu_handle_t *handle_out)
{
    ESP_RETURN_ON_FALSE(handle_out != NULL, ESP_ERR_INVALID_ARG, TAG, "handle_out is null");
    ESP_RETURN_ON_FALSE(!s_imu_open, ESP_ERR_INVALID_STATE, TAG, "imu already open");
    *handle_out = NULL;
    esp_err_t ret = ESP_OK;

    struct bsp_imu_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    i2c_master_bus_handle_t bus = NULL;
    ret = bsp_i2c_acquire(&bus);
    if (ret != ESP_OK) {
        goto err;
    }
    handle->i2c_acquired = true;

    ret = qmi8658_init(&handle->dev, bus, DOERS3_IMU_I2C_ADDR);
    if (ret != ESP_OK) {
        goto err;
    }
    qmi8658_set_accel_unit_mps2(&handle->dev, true);
    qmi8658_set_gyro_unit_rads(&handle->dev, true);

    s_imu_open = true;
    *handle_out = handle;
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
    s_imu_open = false;
    esp_err_t first_err = ESP_OK;
    if (handle->dev.dev_handle != NULL) {
        first_err = i2c_master_bus_rm_device(handle->dev.dev_handle);
    }
    if (handle->i2c_acquired) {
        esp_err_t ret = bsp_i2c_release();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    free(handle);
    return first_err;
}

esp_err_t bsp_imu_read(bsp_imu_handle_t handle, bsp_imu_data_t *data_out)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(data_out != NULL, ESP_ERR_INVALID_ARG, TAG, "data_out is null");

    qmi8658_data_t data = {0};
    ESP_RETURN_ON_ERROR(qmi8658_read_sensor_data(&handle->dev, &data), TAG, "read sensor failed");
    data_out->accel_mps2_x = data.accelX;
    data_out->accel_mps2_y = data.accelY;
    data_out->accel_mps2_z = data.accelZ;
    data_out->gyro_rads_x = data.gyroX;
    data_out->gyro_rads_y = data.gyroY;
    data_out->gyro_rads_z = data.gyroZ;
    data_out->temperature_c = data.temperature;
    data_out->timestamp_ticks = data.timestamp_ticks;
    return ESP_OK;
}

esp_err_t bsp_imu_is_data_ready(bsp_imu_handle_t handle, bool *ready_out)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(ready_out != NULL, ESP_ERR_INVALID_ARG, TAG, "ready_out is null");
    return qmi8658_is_data_ready(&handle->dev, ready_out);
}

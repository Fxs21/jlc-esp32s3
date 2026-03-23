#include <stdlib.h>

#include "bsp_imu_qmi8658.h"
#include "bsp_i2c_bus.h"
#include "esp_check.h"
#include "qmi8658.h"

#define BSP_IMU_TAG "bsp_imu"

typedef struct {
    const bsp_imu_qmi8658_cfg_t *cfg;
    qmi8658_dev_t dev;
    bool inited;
    bool bus_acquired;
} bsp_imu_qmi8658_ctx_t;

esp_err_t bsp_imu_qmi8658_create(const void *config, void **out_ctx)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "config is null");
    ESP_RETURN_ON_FALSE(out_ctx != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "out_ctx is null");

    const bsp_imu_qmi8658_cfg_t *cfg = config;
    bsp_imu_qmi8658_ctx_t *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_NO_MEM, BSP_IMU_TAG, "no memory");
    ctx->cfg = cfg;

    bsp_i2c_bus_cfg_t bus_cfg = bsp_i2c_bus_default_cfg();
    esp_err_t ret = bsp_i2c_bus_open(&bus_cfg);
    if (ret != ESP_OK) {
        free(ctx);
        return ret;
    }
    ctx->bus_acquired = true;

    i2c_master_bus_handle_t i2c_bus = NULL;
    ret = bsp_i2c_bus_get(&i2c_bus);
    if (ret != ESP_OK) {
        (void)bsp_imu_qmi8658_destroy(ctx);
        return ret;
    }

    ret = qmi8658_init(&ctx->dev, i2c_bus, cfg->address);
    if (ret != ESP_OK) {
        (void)bsp_imu_qmi8658_destroy(ctx);
        return ret;
    }

    qmi8658_set_accel_unit_mps2(&ctx->dev, cfg->accel_unit_mps2);
    qmi8658_set_gyro_unit_rads(&ctx->dev, cfg->gyro_unit_rads);
    qmi8658_set_display_precision(&ctx->dev, cfg->display_precision);

    ctx->inited = true;
    *out_ctx = ctx;
    return ESP_OK;
}

esp_err_t bsp_imu_qmi8658_destroy(void *ctx_ptr)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "ctx is null");

    bsp_imu_qmi8658_ctx_t *ctx = ctx_ptr;
    esp_err_t first_err = ESP_OK;
    if (ctx->bus_acquired) {
        first_err = bsp_i2c_bus_close();
        ctx->bus_acquired = false;
    }
    free(ctx);
    return first_err;
}

esp_err_t bsp_imu_qmi8658_read(void *ctx_ptr, bsp_imu_data_t *out_data)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "ctx is null");
    ESP_RETURN_ON_FALSE(out_data != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "out_data is null");

    bsp_imu_qmi8658_ctx_t *ctx = ctx_ptr;
    ESP_RETURN_ON_FALSE(ctx->inited, ESP_ERR_INVALID_STATE, BSP_IMU_TAG, "imu not initialized");

    qmi8658_data_t data = { 0 };
    ESP_RETURN_ON_ERROR(qmi8658_read_sensor_data(&ctx->dev, &data), BSP_IMU_TAG, "read sensor data failed");

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

esp_err_t bsp_imu_qmi8658_is_data_ready(void *ctx_ptr, bool *out_ready)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "ctx is null");
    ESP_RETURN_ON_FALSE(out_ready != NULL, ESP_ERR_INVALID_ARG, BSP_IMU_TAG, "out_ready is null");

    bsp_imu_qmi8658_ctx_t *ctx = ctx_ptr;
    ESP_RETURN_ON_FALSE(ctx->inited, ESP_ERR_INVALID_STATE, BSP_IMU_TAG, "imu not initialized");
    return qmi8658_is_data_ready(&ctx->dev, out_ready);
}

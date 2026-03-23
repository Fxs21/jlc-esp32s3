#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_imu_s *bsp_imu_handle_t;

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float temperature;
    uint32_t timestamp;
} bsp_imu_data_t;

bool bsp_imu_present(void);

esp_err_t bsp_imu_new(bsp_imu_handle_t *out_handle);
esp_err_t bsp_imu_del(bsp_imu_handle_t handle);

esp_err_t bsp_imu_read(bsp_imu_handle_t handle, bsp_imu_data_t *out_data);
esp_err_t bsp_imu_is_data_ready(bsp_imu_handle_t handle, bool *out_ready);

#ifdef __cplusplus
}
#endif

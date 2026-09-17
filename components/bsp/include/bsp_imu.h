#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_imu_s *bsp_imu_handle_t;

typedef struct {
    bool present;
} bsp_imu_desc_t;

typedef struct {
    float accel_mps2_x;
    float accel_mps2_y;
    float accel_mps2_z;
    float gyro_rads_x;
    float gyro_rads_y;
    float gyro_rads_z;
    float temperature_c;
    // Monotonic sensor sample counter; not a time unit, rate follows the configured ODR.
    uint32_t timestamp_ticks;
} bsp_imu_data_t;

const bsp_imu_desc_t *bsp_imu_get_desc(void);

// On failure, *handle_out is set to NULL after handle_out is validated.
esp_err_t bsp_imu_open(bsp_imu_handle_t *handle_out);
esp_err_t bsp_imu_close(bsp_imu_handle_t handle);

esp_err_t bsp_imu_read(bsp_imu_handle_t handle, bsp_imu_data_t *data_out);
esp_err_t bsp_imu_is_data_ready(bsp_imu_handle_t handle, bool *ready_out);

#ifdef __cplusplus
}
#endif

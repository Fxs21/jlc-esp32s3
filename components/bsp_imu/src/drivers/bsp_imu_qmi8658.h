#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp_imu_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t address;
    bool accel_unit_mps2;
    bool gyro_unit_rads;
    uint8_t display_precision;
} bsp_imu_qmi8658_cfg_t;

esp_err_t bsp_imu_qmi8658_create(const void *config, void **out_ctx);
esp_err_t bsp_imu_qmi8658_destroy(void *ctx);
esp_err_t bsp_imu_qmi8658_read(void *ctx, bsp_imu_data_t *out_data);
esp_err_t bsp_imu_qmi8658_is_data_ready(void *ctx, bool *out_ready);

#ifdef __cplusplus
}
#endif

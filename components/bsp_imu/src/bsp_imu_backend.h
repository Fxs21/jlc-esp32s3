#pragma once

#include "bsp_imu.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const void *config;
    esp_err_t (*create)(const void *config, void **out_ctx);
    esp_err_t (*destroy)(void *ctx);
    esp_err_t (*read)(void *ctx, bsp_imu_data_t *out_data);
    esp_err_t (*is_data_ready)(void *ctx, bool *out_ready);
} bsp_imu_backend_t;

const bsp_imu_backend_t *bsp_imu_backend_get(void);

#ifdef __cplusplus
}
#endif

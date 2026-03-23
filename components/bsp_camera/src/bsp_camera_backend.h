#pragma once

#include "bsp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const void *config;
    esp_err_t (*create)(const void *config, void **out_ctx);
    esp_err_t (*start)(void *ctx);
    esp_err_t (*fb_get)(void *ctx, camera_fb_t **out_frame);
    esp_err_t (*fb_return)(void *ctx, camera_fb_t *frame);
    esp_err_t (*stop)(void *ctx);
    sensor_t *(*sensor)(void *ctx);
    esp_err_t (*destroy)(void *ctx);
} bsp_camera_backend_t;

const bsp_camera_backend_t *bsp_camera_backend_get(void);

#ifdef __cplusplus
}
#endif

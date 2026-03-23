#pragma once

#include "bsp_backlight.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const void *config;
    esp_err_t (*create)(const void *config, void **out_ctx);
    esp_err_t (*destroy)(void *ctx);
    esp_err_t (*set_percent)(void *ctx, uint8_t percent);
} bsp_backlight_backend_t;

const bsp_backlight_backend_t *bsp_backlight_backend_get(void);

#ifdef __cplusplus
}
#endif

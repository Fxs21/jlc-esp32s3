#pragma once

#include "bsp_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const void *config;
    esp_err_t (*create)(const void *config, void **out_ctx);
    esp_err_t (*destroy)(void *ctx);
    esp_err_t (*read)(void *ctx,
                      esp_lcd_touch_point_data_t *points,
                      size_t points_capacity,
                      size_t *points_num);
} bsp_touch_backend_t;

const bsp_touch_backend_t *bsp_touch_backend_get(void);

#ifdef __cplusplus
}
#endif

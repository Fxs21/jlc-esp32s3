#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_touch_s *bsp_touch_handle_t;

typedef struct {
    bool present;
    uint8_t max_points;
    uint16_t x_max;
    uint16_t y_max;
} bsp_touch_desc_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t pressure;
    uint8_t id;
} bsp_touch_point_t;

const bsp_touch_desc_t *bsp_touch_get_desc(void);

// On failure, *out_handle is set to NULL after out_handle is validated.
esp_err_t bsp_touch_open(bsp_touch_handle_t *out_handle);
esp_err_t bsp_touch_close(bsp_touch_handle_t handle);
esp_err_t bsp_touch_read(bsp_touch_handle_t handle,
                         bsp_touch_point_t *points,
                         size_t points_capacity,
                         size_t *points_num);

#ifdef __cplusplus
}
#endif

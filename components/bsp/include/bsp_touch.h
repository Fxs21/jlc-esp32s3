#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_touch_s *bsp_touch_handle_t;

typedef struct {
    bool present;
} bsp_touch_desc_t;

const bsp_touch_desc_t *bsp_touch_get_desc(void);

esp_err_t bsp_touch_open(bsp_touch_handle_t *out_handle);
esp_err_t bsp_touch_close(bsp_touch_handle_t handle);
esp_err_t bsp_touch_read(bsp_touch_handle_t handle,
                         esp_lcd_touch_point_data_t *points,
                         size_t points_capacity,
                         size_t *points_num);

#ifdef __cplusplus
}
#endif

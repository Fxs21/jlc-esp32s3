#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *buf1;
    void *buf2;
    size_t size;
    uint16_t lines;
    const char *memory;
} bsp_lvgl_buffer_t;

esp_err_t bsp_lvgl_buffer_alloc(bsp_lvgl_buffer_t *out,
                                uint16_t width,
                                uint16_t height,
                                uint8_t bytes_per_pixel);
void bsp_lvgl_buffer_free(bsp_lvgl_buffer_t *buffer);

#ifdef __cplusplus
}
#endif

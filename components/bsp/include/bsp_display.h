#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Low-level native display transfer API. Pixel byte order is board-native;
// application UIs should use bsp_ui_* instead of assuming a common format.
typedef struct bsp_display_s *bsp_display_handle_t;

typedef struct {
    bool present;
    uint16_t width;
    uint16_t height;
    uint8_t bits_per_pixel;
    uint16_t align_x;
    uint16_t align_y;
    size_t max_transfer_bytes;
} bsp_display_info_t;

typedef bool (*bsp_display_transfer_done_cb_t)(void *user_ctx);

// On failure, *out_handle is set to NULL after out_handle is validated.
esp_err_t bsp_display_open(bsp_display_handle_t *out_handle);
esp_err_t bsp_display_close(bsp_display_handle_t handle);
const bsp_display_info_t *bsp_display_get_info(bsp_display_handle_t handle);

esp_err_t bsp_display_set_transfer_done_cb(bsp_display_handle_t handle,
                                           bsp_display_transfer_done_cb_t cb,
                                           void *user_ctx);
esp_err_t bsp_display_write_async(bsp_display_handle_t handle,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  const void *data,
                                  size_t data_size);
esp_err_t bsp_display_wait_transfer_done(bsp_display_handle_t handle);

#ifdef __cplusplus
}
#endif

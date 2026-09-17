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
} bsp_display_info_t;

typedef bool (*bsp_display_transfer_done_cb_t)(void *user_ctx);

// On failure, *handle_out is set to NULL after handle_out is validated.
esp_err_t bsp_display_open(bsp_display_handle_t *handle_out);
esp_err_t bsp_display_close(bsp_display_handle_t handle);
const bsp_display_info_t *bsp_display_get_info(bsp_display_handle_t handle);

esp_err_t bsp_display_set_done_cb(bsp_display_handle_t handle,
                                  bsp_display_transfer_done_cb_t cb,
                                  void *user_ctx);
esp_err_t bsp_display_write(bsp_display_handle_t handle,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  const void *data,
                                  size_t data_size);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_camera_s *bsp_camera_handle_t;

typedef enum {
    BSP_CAMERA_PIXEL_FORMAT_RGB565 = 0,
} bsp_camera_pixel_format_t;

typedef struct {
    bool present;
    uint16_t width;
    uint16_t height;
    bsp_camera_pixel_format_t pixel_format;
} bsp_camera_desc_t;

typedef struct {
    const uint8_t *data;
    size_t len;
    uint16_t width;
    uint16_t height;
    bsp_camera_pixel_format_t pixel_format;
    uint32_t stride_bytes;
    uint64_t timestamp_us;
} bsp_camera_frame_t;

const bsp_camera_desc_t *bsp_camera_get_desc(void);
// On failure, *out_handle is set to NULL after out_handle is validated.
esp_err_t bsp_camera_open(bsp_camera_handle_t *out_handle);
esp_err_t bsp_camera_close(bsp_camera_handle_t handle);
// Capture a frame. The returned data remains owned by the BSP and is valid until
// bsp_camera_release_frame() or bsp_camera_close(). A second capture is rejected
// until the active frame is released. This call may block while waiting for a frame.
esp_err_t bsp_camera_capture(bsp_camera_handle_t handle, bsp_camera_frame_t *out_frame);
esp_err_t bsp_camera_release_frame(bsp_camera_handle_t handle, const bsp_camera_frame_t *frame);

#ifdef __cplusplus
}
#endif

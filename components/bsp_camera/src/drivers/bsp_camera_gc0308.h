#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp_camera_backend.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_CAMERA_CTRL_PIN_NONE = 0,
    BSP_CAMERA_CTRL_PIN_GPIO,
    BSP_CAMERA_CTRL_PIN_IOEXP,
} bsp_camera_ctrl_pin_type_t;

typedef struct {
    bsp_camera_ctrl_pin_type_t type;
    bool active_high;
    union {
        gpio_num_t gpio_num;
        uint8_t ioexp_num;
    } num;
} bsp_camera_ctrl_pin_t;

typedef struct {
    bsp_camera_ctrl_pin_t pwdn;
    gpio_num_t reset;
    gpio_num_t xclk;
    gpio_num_t d7;
    gpio_num_t d6;
    gpio_num_t d5;
    gpio_num_t d4;
    gpio_num_t d3;
    gpio_num_t d2;
    gpio_num_t d1;
    gpio_num_t d0;
    gpio_num_t vsync;
    gpio_num_t href;
    gpio_num_t pclk;
    uint32_t xclk_freq_hz;
    pixformat_t pixel_format;
    framesize_t frame_size;
    int jpeg_quality;
    int fb_count;
    camera_fb_location_t fb_location;
    camera_grab_mode_t grab_mode;
    bool enable_hmirror;
    bool enable_vflip;
} bsp_camera_gc0308_cfg_t;

esp_err_t bsp_camera_gc0308_create(const void *config, void **out_ctx);
esp_err_t bsp_camera_gc0308_start(void *ctx);
esp_err_t bsp_camera_gc0308_fb_get(void *ctx, camera_fb_t **out_frame);
esp_err_t bsp_camera_gc0308_fb_return(void *ctx, camera_fb_t *frame);
esp_err_t bsp_camera_gc0308_stop(void *ctx);
sensor_t *bsp_camera_gc0308_sensor(void *ctx);
esp_err_t bsp_camera_gc0308_destroy(void *ctx);

#ifdef __cplusplus
}
#endif

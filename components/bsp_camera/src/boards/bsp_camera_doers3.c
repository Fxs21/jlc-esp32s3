#include "bsp_camera_backend.h"
#include "bsp_camera_gc0308.h"

static const bsp_camera_gc0308_cfg_t doers3_camera_cfg = {
    .pwdn = {
        .type = BSP_CAMERA_CTRL_PIN_IOEXP,
        .active_high = true,
        .num.ioexp_num = 2,
    },
    .reset = GPIO_NUM_NC,
    .xclk = GPIO_NUM_5,
    .d7 = GPIO_NUM_9,
    .d6 = GPIO_NUM_4,
    .d5 = GPIO_NUM_6,
    .d4 = GPIO_NUM_15,
    .d3 = GPIO_NUM_17,
    .d2 = GPIO_NUM_8,
    .d1 = GPIO_NUM_18,
    .d0 = GPIO_NUM_16,
    .vsync = GPIO_NUM_3,
    .href = GPIO_NUM_46,
    .pclk = GPIO_NUM_7,
    .xclk_freq_hz = 24000000,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    .enable_hmirror = true,
    .enable_vflip = false,
};

const bsp_camera_backend_t bsp_camera_backend_doers3 = {
    .config = &doers3_camera_cfg,
    .create = bsp_camera_gc0308_create,
    .start = bsp_camera_gc0308_start,
    .fb_get = bsp_camera_gc0308_fb_get,
    .fb_return = bsp_camera_gc0308_fb_return,
    .stop = bsp_camera_gc0308_stop,
    .sensor = bsp_camera_gc0308_sensor,
    .destroy = bsp_camera_gc0308_destroy,
};

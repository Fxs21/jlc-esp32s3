#include "bsp_camera_backend.h"
#include "bsp_boarddb.h"
#include "bsp_camera_gc0308.h"

static bsp_camera_ctrl_pin_t ctrl_pin_from_boarddb(bsp_boarddb_ctrl_pin_t pin)
{
    bsp_camera_ctrl_pin_t out = {
        .type = (bsp_camera_ctrl_pin_type_t)pin.type,
        .active_high = pin.active_high,
    };
    if (pin.type == BSP_BOARDDB_CTRL_PIN_GPIO) {
        out.num.gpio_num = pin.num.gpio_num;
    } else {
        out.num.ioexp_num = pin.num.ioexp_num;
    }
    return out;
}

const bsp_camera_backend_t *bsp_camera_backend_get(void)
{
    static bsp_camera_gc0308_cfg_t cfg;
    static bsp_camera_backend_t backend;
    static bool initialized;

    const bsp_boarddb_camera_desc_t *camera = &bsp_boarddb_get()->camera;
    if (!camera->present) {
        return NULL;
    }

    if (!initialized) {
        cfg.pwdn = ctrl_pin_from_boarddb(camera->pwdn);
        cfg.reset = camera->reset;
        cfg.xclk = camera->xclk;
        cfg.d7 = camera->d7;
        cfg.d6 = camera->d6;
        cfg.d5 = camera->d5;
        cfg.d4 = camera->d4;
        cfg.d3 = camera->d3;
        cfg.d2 = camera->d2;
        cfg.d1 = camera->d1;
        cfg.d0 = camera->d0;
        cfg.vsync = camera->vsync;
        cfg.href = camera->href;
        cfg.pclk = camera->pclk;
        cfg.xclk_freq_hz = camera->xclk_freq_hz;
        cfg.pixel_format = camera->pixel_format;
        cfg.frame_size = camera->frame_size;
        cfg.jpeg_quality = camera->jpeg_quality;
        cfg.fb_count = camera->fb_count;
        cfg.fb_location = camera->fb_location;
        cfg.grab_mode = camera->grab_mode;
        cfg.enable_hmirror = camera->enable_hmirror;
        cfg.enable_vflip = camera->enable_vflip;

        backend.config = &cfg;
        backend.create = bsp_camera_gc0308_create;
        backend.start = bsp_camera_gc0308_start;
        backend.fb_get = bsp_camera_gc0308_fb_get;
        backend.fb_return = bsp_camera_gc0308_fb_return;
        backend.stop = bsp_camera_gc0308_stop;
        backend.sensor = bsp_camera_gc0308_sensor;
        backend.destroy = bsp_camera_gc0308_destroy;
        initialized = true;
    }

    return &backend;
}

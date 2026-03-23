#include "bsp_touch_backend.h"

#include "bsp_boarddb.h"
#include "bsp_touch_ft5x06.h"

static esp_err_t ft5x06_create(const void *config, void **out_ctx)
{
    bsp_touch_driver_handle_t handle = NULL;
    esp_err_t ret = bsp_touch_ft5x06_new((const bsp_touch_ft5x06_cfg_t *)config, &handle);
    if (ret == ESP_OK) {
        *out_ctx = handle;
    }
    return ret;
}

static esp_err_t ft5x06_destroy(void *ctx)
{
    return bsp_touch_ft5x06_del((bsp_touch_driver_handle_t)ctx);
}

static esp_err_t ft5x06_read(void *ctx,
                             esp_lcd_touch_point_data_t *points,
                             size_t points_capacity,
                             size_t *points_num)
{
    return bsp_touch_ft5x06_read((bsp_touch_driver_handle_t)ctx, points, points_capacity, points_num);
}

const bsp_touch_backend_t *bsp_touch_backend_get(void)
{
    static bsp_touch_ft5x06_cfg_t ft5x06_cfg;
    static bsp_touch_backend_t backend;
    static bool initialized;

    const bsp_boarddb_touch_desc_t *touch = &bsp_boarddb_get()->touch;
    if (!touch->present) {
        return NULL;
    }

    switch (touch->kind) {
    case BSP_BOARDDB_TOUCH_KIND_FT5X06:
        if (!initialized) {
            ft5x06_cfg.address = touch->address;
            ft5x06_cfg.x_max = touch->x_max;
            ft5x06_cfg.y_max = touch->y_max;
            ft5x06_cfg.reset_gpio = touch->reset_gpio;
            ft5x06_cfg.int_gpio = touch->int_gpio;
            ft5x06_cfg.level_reset = touch->level_reset;
            ft5x06_cfg.level_interrupt = touch->level_interrupt;
            ft5x06_cfg.swap_xy = touch->swap_xy;
            ft5x06_cfg.mirror_x = touch->mirror_x;
            ft5x06_cfg.mirror_y = touch->mirror_y;

            backend.config = &ft5x06_cfg;
            backend.create = ft5x06_create;
            backend.destroy = ft5x06_destroy;
            backend.read = ft5x06_read;
            initialized = true;
        }
        return &backend;
    case BSP_BOARDDB_TOUCH_KIND_NONE:
    default:
        return NULL;
    }
}

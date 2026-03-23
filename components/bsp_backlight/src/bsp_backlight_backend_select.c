#include "bsp_backlight_backend.h"

#include "bsp_backlight_ledc.h"
#include "bsp_boarddb.h"

static esp_err_t ledc_create(const void *config, void **out_ctx)
{
    bsp_backlight_driver_handle_t handle = NULL;
    esp_err_t ret = bsp_backlight_ledc_new((const bsp_backlight_ledc_cfg_t *)config, &handle);
    if (ret == ESP_OK) {
        *out_ctx = handle;
    }
    return ret;
}

static esp_err_t ledc_destroy(void *ctx)
{
    return bsp_backlight_ledc_del((bsp_backlight_driver_handle_t)ctx);
}

static esp_err_t ledc_set_percent(void *ctx, uint8_t percent)
{
    return bsp_backlight_ledc_set_percent((bsp_backlight_driver_handle_t)ctx, percent);
}

const bsp_backlight_backend_t *bsp_backlight_backend_get(void)
{
    static bsp_backlight_ledc_cfg_t ledc_cfg;
    static bsp_backlight_backend_t backend;
    static bool initialized;

    const bsp_boarddb_backlight_desc_t *backlight = &bsp_boarddb_get()->backlight;
    if (!backlight->present) {
        return NULL;
    }

    switch (backlight->kind) {
    case BSP_BOARDDB_BACKLIGHT_KIND_LEDC:
        if (!initialized) {
            ledc_cfg.gpio = backlight->gpio;
            ledc_cfg.output_invert = backlight->output_invert;

            backend.config = &ledc_cfg;
            backend.create = ledc_create;
            backend.destroy = ledc_destroy;
            backend.set_percent = ledc_set_percent;
            initialized = true;
        }
        return &backend;
    case BSP_BOARDDB_BACKLIGHT_KIND_NONE:
    default:
        return NULL;
    }
}

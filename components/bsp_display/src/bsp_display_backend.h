#pragma once

#include "bsp_display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const void *config;
    esp_err_t (*create)(const void *config, void **out_ctx);
    esp_err_t (*destroy)(void *ctx);
    esp_err_t (*draw_bitmap)(void *ctx, int x_start, int y_start, int x_end, int y_end, const void *color_data);
    esp_err_t (*flush_wait)(void *ctx);
    esp_err_t (*register_flush_done_cb)(void *ctx,
                                        esp_lcd_panel_io_color_trans_done_cb_t cb,
                                        void *user_ctx);
} bsp_display_backend_t;

const bsp_display_backend_t *bsp_display_backend_get(void);

#ifdef __cplusplus
}
#endif

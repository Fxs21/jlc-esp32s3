#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t auras3_panel_acquire(esp_lcd_panel_handle_t *out_panel, esp_lcd_panel_io_handle_t *out_io);
esp_err_t auras3_panel_release(void);
esp_err_t auras3_panel_set_transfer_done_cb(esp_lcd_panel_io_color_trans_done_cb_t cb, void *user_ctx);
esp_err_t auras3_panel_prepare_transfer(void);
esp_err_t auras3_panel_wait_transfer_done(void);
esp_err_t auras3_panel_set_brightness(uint8_t percent);
esp_err_t auras3_panel_get_brightness(uint8_t *out_percent);

#ifdef __cplusplus
}
#endif

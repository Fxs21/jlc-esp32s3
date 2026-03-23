#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp_display.h"
#include "bsp_lvgl_buffer.h"
#include "esp_lcd_panel_io.h"
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Embedded in each board's struct bsp_display_s.
// Board port initialises the panel-specific fields; this port struct
// holds the LVGL state and transfer-done callback that are common to
// all board display implementations.
typedef struct {
    lv_display_t *lv_display;
    bsp_lvgl_buffer_t lv_buffer;
    bsp_display_transfer_done_cb_t transfer_done_cb;
    void *transfer_done_user_ctx;
} bsp_lvgl_port_t;

// Bridge from esp_lcd_panel_io callback to bsp_display_transfer_done_cb_t.
// Replaces the board-local transfer_done_bridge_cb in display.c.
bool bsp_lvgl_transfer_done_bridge(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx);

// Allocate LVGL display, buffers and set partial render mode.
// user_data is stored via lv_display_set_user_data() so the board-specific
// flush callback can recover its full struct bsp_display_s handle.
esp_err_t bsp_lvgl_port_open(bsp_lvgl_port_t *port, uint16_t width, uint16_t height,
                              void *user_data);

// Tear down the LVGL port created by bsp_lvgl_port_open().
esp_err_t bsp_lvgl_port_close(bsp_lvgl_port_t *port, lv_display_t *display);

#ifdef __cplusplus
}
#endif

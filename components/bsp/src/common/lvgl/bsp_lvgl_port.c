#include "bsp_lvgl_port.h"

#include "esp_check.h"

#define TAG "bsp_lvgl_port"

bool bsp_lvgl_transfer_done_bridge(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    bsp_lvgl_port_t *port = (bsp_lvgl_port_t *)user_ctx;
    if (port == NULL || port->transfer_done_cb == NULL) {
        return false;
    }
    return port->transfer_done_cb(port->transfer_done_user_ctx);
}

esp_err_t bsp_lvgl_port_open(bsp_lvgl_port_t *port, uint16_t width, uint16_t height,
                              void *user_data)
{
    ESP_RETURN_ON_FALSE(port != NULL, ESP_ERR_INVALID_ARG, TAG, "port is null");
    ESP_RETURN_ON_FALSE(port->lv_display == NULL, ESP_ERR_INVALID_STATE, TAG, "LVGL port already open");

    port->lv_display = lv_display_create(width, height);
    ESP_RETURN_ON_FALSE(port->lv_display != NULL, ESP_ERR_NO_MEM, TAG, "create display failed");

    lv_display_set_color_format(port->lv_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(port->lv_display, user_data);

    esp_err_t ret = bsp_lvgl_buffer_alloc(&port->lv_buffer, width, height, sizeof(lv_color16_t));
    if (ret != ESP_OK) {
        lv_display_delete(port->lv_display);
        port->lv_display = NULL;
        return ret;
    }

    lv_display_set_buffers(port->lv_display,
                           port->lv_buffer.buf1,
                           port->lv_buffer.buf2,
                           port->lv_buffer.size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return ESP_OK;
}

esp_err_t bsp_lvgl_port_close(bsp_lvgl_port_t *port, lv_display_t *display)
{
    ESP_RETURN_ON_FALSE(port != NULL, ESP_ERR_INVALID_ARG, TAG, "port is null");
    ESP_RETURN_ON_FALSE(display != NULL, ESP_ERR_INVALID_ARG, TAG, "display is null");
    ESP_RETURN_ON_FALSE(display == port->lv_display, ESP_ERR_INVALID_ARG, TAG, "display mismatch");

    lv_display_delete(display);
    port->lv_display = NULL;
    bsp_lvgl_buffer_free(&port->lv_buffer);
    return ESP_OK;
}

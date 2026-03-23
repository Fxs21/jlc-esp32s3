#include "bsp_display.h"

#include "esp_check.h"

#define TAG "bsp_display"

static const bsp_display_desc_t s_desc = {
    .present = false,
};

const bsp_display_desc_t *bsp_display_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_display_close(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

esp_err_t bsp_display_draw_bitmap(bsp_display_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    (void)x_start;
    (void)y_start;
    (void)x_end;
    (void)y_end;
    (void)color_data;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_display_fill(bsp_display_handle_t handle, uint16_t color)
{
    (void)color;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_display_register_flush_done_cb(bsp_display_handle_t handle, esp_lcd_panel_io_color_trans_done_cb_t cb, void *user_ctx)
{
    (void)cb;
    (void)user_ctx;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

#include "bsp_display.h"
#include <stdlib.h>

#include "bsp_boarddb.h"
#include "bsp_display_backend.h"
#include "esp_check.h"
#include "esp_heap_caps.h"

#define BSP_DISPLAY_TAG "bsp_display"

struct bsp_display_s {
    const bsp_display_backend_t *backend;
    void *ctx;
    uint16_t *fill_line_buf;
};

static bsp_display_desc_t display_desc;

const bsp_display_desc_t *bsp_display_get_desc(void)
{
    const bsp_boarddb_display_desc_t *display = &bsp_boarddb_get()->display;
    display_desc.present = (bsp_display_backend_get() != NULL);
    display_desc.has_backlight = bsp_boarddb_get()->backlight.present;
    display_desc.has_touch = bsp_boarddb_get()->touch.present;
    display_desc.width = display->width;
    display_desc.height = display->height;
    display_desc.data_endian = display->data_endian;
    return &display_desc;
}

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "out_handle is null");
    esp_err_t ret = ESP_OK;

    struct bsp_display_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_DISPLAY_TAG, "no memory");

    handle->backend = bsp_display_backend_get();
    ESP_GOTO_ON_FALSE(handle->backend != NULL,
                      ESP_ERR_NOT_SUPPORTED,
                      err,
                      BSP_DISPLAY_TAG,
                      "display backend not configured");

    ret = handle->backend->create(handle->backend->config, &handle->ctx);
    if (ret != ESP_OK) {
        goto err;
    }

    const bsp_boarddb_display_desc_t *display = &bsp_boarddb_get()->display;
    size_t fill_bytes = display->width * sizeof(uint16_t);
    handle->fill_line_buf = heap_caps_malloc(fill_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (handle->fill_line_buf == NULL) {
        handle->fill_line_buf = heap_caps_malloc(fill_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    ESP_GOTO_ON_FALSE(handle->fill_line_buf != NULL, ESP_ERR_NO_MEM, err, BSP_DISPLAY_TAG, "fill buf alloc failed");

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)bsp_display_close(handle);
    }
    return ret;
}

esp_err_t bsp_display_close(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    esp_err_t ret = ESP_OK;
    if (handle->fill_line_buf != NULL) {
        heap_caps_free(handle->fill_line_buf);
    }
    if (handle->backend != NULL && handle->ctx != NULL) {
        ret = handle->backend->destroy(handle->ctx);
    }
    free(handle);
    return ret;
}

esp_err_t bsp_display_draw_bitmap(bsp_display_handle_t handle,
                                  int x_start,
                                  int y_start,
                                  int x_end,
                                  int y_end,
                                  const void *color_data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    return handle->backend->draw_bitmap(handle->ctx, x_start, y_start, x_end, y_end, color_data);
}

esp_err_t bsp_display_fill(bsp_display_handle_t handle, uint16_t color)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    const bsp_display_desc_t *desc = bsp_display_get_desc();
    ESP_RETURN_ON_FALSE(handle->fill_line_buf != NULL, ESP_ERR_INVALID_STATE, BSP_DISPLAY_TAG, "fill buffer missing");
    for (uint16_t x = 0; x < desc->width; ++x) {
        handle->fill_line_buf[x] = color;
    }
    for (uint16_t y = 0; y < desc->height; ++y) {
        ESP_RETURN_ON_ERROR(bsp_display_draw_bitmap(handle, 0, y, desc->width, y + 1, handle->fill_line_buf),
                            BSP_DISPLAY_TAG,
                            "fill draw failed");
    }
    return handle->backend->flush_wait(handle->ctx);
}

esp_err_t bsp_display_register_flush_done_cb(bsp_display_handle_t handle,
                                             esp_lcd_panel_io_color_trans_done_cb_t cb,
                                             void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_DISPLAY_TAG, "handle is null");
    return handle->backend->register_flush_done_cb(handle->ctx, cb, user_ctx);
}

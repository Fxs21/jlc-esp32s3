#include "bsp_lvgl_buffer.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define TAG "bsp_lvgl_buffer"
#define BUFFER_HEIGHT_DIVISOR 8

static uint16_t buffer_lines(uint16_t height)
{
    return (height + BUFFER_HEIGHT_DIVISOR - 1) / BUFFER_HEIGHT_DIVISOR;
}

static bool alloc_pair(bsp_lvgl_buffer_t *out, size_t size, uint32_t caps, const char *memory)
{
    out->buf1 = heap_caps_malloc(size, caps);
    out->buf2 = heap_caps_malloc(size, caps);
    if (out->buf1 == NULL || out->buf2 == NULL) {
        bsp_lvgl_buffer_free(out);
        return false;
    }
    out->size = size;
    out->memory = memory;
    return true;
}

esp_err_t bsp_lvgl_buffer_alloc(bsp_lvgl_buffer_t *out,
                                uint16_t width,
                                uint16_t height,
                                uint8_t bytes_per_pixel)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "out is null");
    ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "invalid size");
    ESP_RETURN_ON_FALSE(bytes_per_pixel > 0, ESP_ERR_INVALID_ARG, TAG, "invalid pixel size");

    memset(out, 0, sizeof(*out));
    out->lines = buffer_lines(height);
    const size_t size = (size_t)width * out->lines * bytes_per_pixel;

    // Snapshot free heap before allocation.
    const size_t sram_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    // BSP default: double buffer, screen height / 8, SRAM DMA first, PSRAM fallback.
    if (alloc_pair(out, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT, "SRAM DMA")) {
        const size_t sram_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "bufs=%u lines=%u total=%u %s",
                 (unsigned)2, (unsigned)out->lines, (unsigned)(size * 2), out->memory);
        ESP_LOGI(TAG, "before: SRAM=%zu PSRAM=%zu  after: SRAM=%zu PSRAM=%zu  delta: SRAM=%zu PSRAM=%zu",
                 sram_before, psram_before,
                 sram_after, psram_after,
                 sram_before - sram_after, psram_before - psram_after);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "SRAM DMA LVGL buffer failed, falling back to PSRAM");
    const size_t sram_fail = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t psram_fail = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(alloc_pair(out, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, "PSRAM"),
                        ESP_ERR_NO_MEM,
                        TAG,
                        "no LVGL double buffer: lines=%u one=%u total=%u",
                        (unsigned)out->lines,
                        (unsigned)size,
                        (unsigned)(size * 2));
    const size_t psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t sram_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "bufs=%u lines=%u total=%u %s",
             (unsigned)2, (unsigned)out->lines, (unsigned)(size * 2), out->memory);
    ESP_LOGI(TAG, "before: SRAM=%zu PSRAM=%zu  after: SRAM=%zu PSRAM=%zu  delta: SRAM=%zu PSRAM=%zu",
             sram_fail, psram_fail,
             sram_after, psram_after,
             sram_fail - sram_after, psram_fail - psram_after);
    return ESP_OK;
}

void bsp_lvgl_buffer_free(bsp_lvgl_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }
    if (buffer->buf1 != NULL) {
        heap_caps_free(buffer->buf1);
    }
    if (buffer->buf2 != NULL) {
        heap_caps_free(buffer->buf2);
    }
    memset(buffer, 0, sizeof(*buffer));
}

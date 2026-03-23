#include <inttypes.h>
#include <stdbool.h>

#include "bsp_backlight.h"
#include "bsp_camera.h"
#include "bsp_display.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TAG "test_bsp_camera"
#define FLUSH_WAIT_MS 200
#define STATS_INTERVAL_US (1000 * 1000)

static bool on_lcd_flush_done(esp_lcd_panel_io_handle_t panel_io,
                              esp_lcd_panel_io_event_data_t *edata,
                              void *user_ctx)
{
    (void)panel_io;
    (void)edata;

    SemaphoreHandle_t flush_sem = (SemaphoreHandle_t)user_ctx;
    if (flush_sem == NULL) {
        return false;
    }

    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(flush_sem, &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

static esp_err_t draw_camera_frame(bsp_display_handle_t display,
                                   SemaphoreHandle_t flush_sem,
                                   const camera_fb_t *frame)
{
    if (frame->format != PIXFORMAT_RGB565) {
        ESP_LOGE(TAG, "unsupported frame format=%d, expect RGB565", (int)frame->format);
        return ESP_ERR_NOT_SUPPORTED;
    }

    const bsp_display_desc_t *display_desc = bsp_display_get_desc();
    ESP_RETURN_ON_FALSE(display_desc != NULL && display_desc->present,
                        ESP_ERR_NOT_SUPPORTED, TAG, "screen not present");
    const uint16_t lcd_w = display_desc->width;
    const uint16_t lcd_h = display_desc->height;
    const int draw_w = (frame->width < lcd_w) ? frame->width : lcd_w;
    const int draw_h = (frame->height < lcd_h) ? frame->height : lcd_h;
    if (draw_w <= 0 || draw_h <= 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    while (xSemaphoreTake(flush_sem, 0) == pdTRUE) {
        // Drain stale signals before issuing a new flush.
    }

    ESP_RETURN_ON_ERROR(
        bsp_display_draw_bitmap(display, 0, 0, draw_w, draw_h, frame->buf),
        TAG,
        "draw bitmap failed");

    if (xSemaphoreTake(flush_sem, pdMS_TO_TICKS(FLUSH_WAIT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "wait flush done timeout");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

void app_main(void)
{
    bsp_display_handle_t display = NULL;
    bsp_backlight_handle_t backlight = NULL;
    bsp_camera_handle_t camera = NULL;

    SemaphoreHandle_t flush_sem = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(flush_sem != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(bsp_display_open(&display));
    ESP_ERROR_CHECK(bsp_display_register_flush_done_cb(display, on_lcd_flush_done, flush_sem));
    ESP_ERROR_CHECK(bsp_display_fill(display, 0x0000));
    if (bsp_backlight_get_desc()->present) {
        ESP_ERROR_CHECK(bsp_backlight_open(&backlight));
        ESP_ERROR_CHECK(bsp_backlight_set_percent(backlight, 100));
    }

    ESP_ERROR_CHECK(bsp_camera_new(&camera));
    ESP_ERROR_CHECK(bsp_camera_start(camera));

    ESP_LOGI(TAG, "camera stream started, drawing to LCD");
    int64_t t0 = esp_timer_get_time();
    uint32_t frames = 0;

    while (1) {
        camera_fb_t *frame = NULL;
        esp_err_t ret = bsp_camera_fb_get(camera, &frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "fb_get failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        ret = draw_camera_frame(display, flush_sem, frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "draw frame failed: %s", esp_err_to_name(ret));
        }

        ret = bsp_camera_fb_return(camera, frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "fb_return failed: %s", esp_err_to_name(ret));
        }

        frames++;
        int64_t now = esp_timer_get_time();
        if ((now - t0) >= STATS_INTERVAL_US) {
            int64_t elapsed = now - t0;
            uint32_t fps = (uint32_t)((frames * 1000000ULL) / (uint64_t)elapsed);
            ESP_LOGI(TAG, "fps=%" PRIu32 " (%" PRIu32 " frames in %" PRIi64 " us)", fps, frames, elapsed);
            t0 = now;
            frames = 0;
        }
    }
}

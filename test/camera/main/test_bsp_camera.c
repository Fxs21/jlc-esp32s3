#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_backlight.h"
#include "bsp_camera.h"
#include "bsp_display.h"
#include "esp_check.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TAG "test_bsp_camera"

#define MAX_FRAMES   200

static bool transfer_done_cb(void *user_ctx)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)user_ctx;
    BaseType_t higher_woken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &higher_woken);
    return (higher_woken == pdTRUE);
}

void app_main(void)
{
    ESP_LOGI(TAG, "TEST START name=camera+display");

    const bsp_camera_desc_t *cam_desc = bsp_camera_get_desc();
    if (!cam_desc->present) {
        ESP_LOGW(TAG, "TEST SKIP name=camera+display reason=\"camera not present\"");
        return;
    }
    ESP_LOGI(TAG, "camera: %ux%u format=%d", cam_desc->width, cam_desc->height, cam_desc->pixel_format);

    esp_err_t ret = ESP_OK;
    bsp_backlight_handle_t bl = NULL;
    bsp_display_handle_t disp = NULL;
    bsp_camera_handle_t cam = NULL;
    bsp_camera_frame_t frame = {0};
    uint8_t *swap_buf = NULL;
    SemaphoreHandle_t sem = NULL;
    bool backlight_open = false;
    bool display_open = false;
    bool camera_open = false;
    bool frame_active = false;

    ret = bsp_backlight_open(&bl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=camera+display step=backlight_open err=%s", esp_err_to_name(ret));
        return;
    }
    backlight_open = true;
    bsp_backlight_set_percent(bl, 50);

    ret = bsp_display_open(&disp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=camera+display step=display_open err=%s", esp_err_to_name(ret));
        goto out;
    }
    display_open = true;
    const bsp_display_info_t *disp_info = bsp_display_get_info(disp);
    ESP_LOGI(TAG, "display: %ux%u bpp=%u", disp_info->width, disp_info->height, disp_info->bits_per_pixel);

    ret = bsp_camera_open(&cam);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=camera+display step=camera_open err=%s", esp_err_to_name(ret));
        goto out;
    }
    camera_open = true;

    // Pre-compute geometry (frame size is fixed QVGA)
    size_t pixel_count = (size_t)cam_desc->width * cam_desc->height;
    size_t buf_size = pixel_count * 2;
    uint16_t dst_x = (disp_info->width > cam_desc->width)
                         ? (disp_info->width - cam_desc->width) / 2 : 0;
    uint16_t dst_y = (disp_info->height > cam_desc->height)
                         ? (disp_info->height - cam_desc->height) / 2 : 0;

    // Create one binary semaphore for transfer-done signalling
    sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
        ESP_LOGE(TAG, "TEST FAIL name=camera+display step=sem_create err=no memory");
        ret = ESP_ERR_NO_MEM;
        goto out;
    }
    bsp_display_set_done_cb(disp, transfer_done_cb, sem);

    uint32_t frame_count = 0;

    while (frame_count < MAX_FRAMES) {
        // Capture
        ret = bsp_camera_capture(cam, &frame);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "capture failed at frame %" PRIu32 ": %s", frame_count, esp_err_to_name(ret));
            break;
        }
        frame_active = true;

        // Byte-swap: camera big-endian RGB565 -> display little-endian RGB565
        swap_buf = malloc(buf_size);
        if (swap_buf == NULL) {
            ESP_LOGE(TAG, "alloc failed at frame %" PRIu32, frame_count);
            break;
        }
        for (size_t i = 0; i < pixel_count; i++) {
            swap_buf[i * 2 + 0] = frame.data[i * 2 + 1];
            swap_buf[i * 2 + 1] = frame.data[i * 2 + 0];
        }

        // Write to display
        ret = bsp_display_write(disp, dst_x, dst_y, cam_desc->width, cam_desc->height,
                                swap_buf, buf_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "display_write failed at frame %" PRIu32 ": %s",
                     frame_count, esp_err_to_name(ret));
            free(swap_buf);
            swap_buf = NULL;
            break;
        }

        // Wait for DMA transfer to complete
        if (xSemaphoreTake(sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW(TAG, "display write timeout at frame %" PRIu32, frame_count);
        }

        // DMA done: release frame and swap buffer
        bsp_camera_release_frame(cam, &frame);
        frame_active = false;
        free(swap_buf);
        swap_buf = NULL;

        frame_count++;
    }

    ESP_LOGI(TAG, "TEST PASS name=camera+display summary=\"%lu frames displayed\"",
             (unsigned long)frame_count);

out:
    vSemaphoreDelete(sem);
    free(swap_buf);
    if (frame_active) {
        bsp_camera_release_frame(cam, &frame);
    }
    if (camera_open) {
        bsp_camera_close(cam);
    }
    if (backlight_open) {
        bsp_backlight_set_percent(bl, 0);
        bsp_backlight_close(bl);
    }
    if (display_open) {
        bsp_display_close(disp);
    }
}

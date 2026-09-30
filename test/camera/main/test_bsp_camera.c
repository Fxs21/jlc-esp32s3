// test/camera: bsp_camera 取景器通路自检 (capture -> 字节交换 -> display).
// AuraS3 无 camera, 期望结果是 SKIP; 有 camera 的板子跑下面的自动化用例.
#include <inttypes.h>
#include <stdlib.h>

#include "bsp_backlight.h"
#include "bsp_camera.h"
#include "bsp_display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "camera"
// 连续刷的帧数: 太少看不出通路是否稳定, 太多拖长自检时间.
#define MAX_FRAMES 200
#define TRANSFER_TIMEOUT_MS 1000

static const char *TAG = "test_bsp_camera";

static bool transfer_done_cb(void *user_ctx)
{
    SemaphoreHandle_t transfer_done = (SemaphoreHandle_t)user_ctx;
    BaseType_t higher_woken = pdFALSE;
    xSemaphoreGiveFromISR(transfer_done, &higher_woken);
    return (higher_woken == pdTRUE);
}

TEST_CASE("camera: viewfinder captures and displays frames", "[camera]")
{
    const bsp_camera_desc_t *camera_desc = bsp_camera_get_desc();
    TEST_ASSERT_NOT_NULL(camera_desc);
    TEST_ASSERT_TRUE_MESSAGE(camera_desc->present, "camera not present");
    ESP_LOGI(TAG, "camera: %ux%u format=%d",
             camera_desc->width, camera_desc->height, camera_desc->pixel_format);

    bsp_backlight_handle_t backlight = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_open(&backlight));
    bsp_backlight_set_percent(backlight, 50);

    bsp_display_handle_t display = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_open(&display));
    const bsp_display_info_t *display_info = bsp_display_get_info(display);
    TEST_ASSERT_NOT_NULL(display_info);
    ESP_LOGI(TAG, "display: %ux%u bpp=%u",
             display_info->width, display_info->height, display_info->bits_per_pixel);

    bsp_camera_handle_t camera = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_camera_open(&camera));

    SemaphoreHandle_t transfer_done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(transfer_done);
    bsp_display_set_done_cb(display, transfer_done_cb, transfer_done);

    const size_t pixel_count = (size_t)camera_desc->width * camera_desc->height;
    const size_t frame_bytes = pixel_count * 2;
    uint8_t *swapped = malloc(frame_bytes);
    TEST_ASSERT_NOT_NULL(swapped);

    const uint16_t dst_x = (display_info->width > camera_desc->width)
                               ? (uint16_t)((display_info->width - camera_desc->width) / 2) : 0;
    const uint16_t dst_y = (display_info->height > camera_desc->height)
                               ? (uint16_t)((display_info->height - camera_desc->height) / 2) : 0;

    uint32_t displayed = 0;
    uint32_t timeouts = 0;
    for (uint32_t frame_index = 0; frame_index < MAX_FRAMES; frame_index++) {
        bsp_camera_frame_t frame = {0};
        TEST_ASSERT_EQUAL(ESP_OK, bsp_camera_capture(camera, &frame));

        // camera 输出 big-endian RGB565, 面板要 host-order.
        for (size_t i = 0; i < pixel_count; i++) {
            swapped[i * 2 + 0] = frame.data[i * 2 + 1];
            swapped[i * 2 + 1] = frame.data[i * 2 + 0];
        }

        esp_err_t ret = bsp_display_write(display, dst_x, dst_y,
                                          camera_desc->width, camera_desc->height,
                                          swapped, frame_bytes);
        bsp_camera_release_frame(camera, &frame);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        if (xSemaphoreTake(transfer_done, pdMS_TO_TICKS(TRANSFER_TIMEOUT_MS)) != pdTRUE) {
            timeouts++;
        }
        displayed++;
    }

    ESP_LOGI(TAG, "viewfinder: %" PRIu32 " frames, %" PRIu32 " transfer timeouts", displayed, timeouts);
    TEST_ASSERT_EQUAL(0, timeouts);
    TEST_ASSERT_EQUAL(MAX_FRAMES, displayed);

    free(swapped);
    vSemaphoreDelete(transfer_done);
    bsp_camera_close(camera);
    bsp_backlight_set_percent(backlight, 0);
    bsp_backlight_close(backlight);
    bsp_display_close(display);
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_camera_desc_t *camera_desc = bsp_camera_get_desc();
    if (camera_desc == NULL || !camera_desc->present) {
        selftest_skip(MODULE, "no camera on this board");
        return;
    }

    (void)selftest_run(MODULE);
}

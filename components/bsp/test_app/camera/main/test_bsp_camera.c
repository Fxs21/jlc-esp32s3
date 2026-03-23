#include <inttypes.h>
#include <stdio.h>

#include "bsp_camera.h"
#include "esp_check.h"
#include "esp_log.h"

#define TAG "test_bsp_camera"

static uint32_t frame_checksum(const uint8_t *data, size_t len)
{
    uint32_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum = (checksum * 33u) + data[i];
    }
    return checksum;
}

void app_main(void)
{
    ESP_LOGI(TAG, "TEST START name=camera");
    const bsp_camera_desc_t *desc = bsp_camera_get_desc();
    ESP_LOGI(TAG, "camera: present=%d size=%ux%u format=%d",
             desc->present,
             desc->width,
             desc->height,
             desc->pixel_format);
    if (!desc->present) {
        ESP_LOGW(TAG, "TEST SKIP name=camera reason=\"not present\"");
        return;
    }

    esp_err_t ret = ESP_OK;
    bsp_camera_handle_t camera = NULL;
    bsp_camera_frame_t frame = {0};
    bool frame_active = false;

    ret = bsp_camera_open(&camera);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=camera step=open err=%s", esp_err_to_name(ret));
        return;
    }

    ret = bsp_camera_capture(camera, &frame);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=camera step=capture err=%s", esp_err_to_name(ret));
        goto out;
    }
    frame_active = true;

    const uint32_t checksum = frame_checksum(frame.data, frame.len);
    ESP_LOGI(TAG, "frame: %ux%u stride=%" PRIu32 " len=%zu format=%d timestamp_us=%" PRIu64 " checksum=0x%08" PRIx32,
             frame.width,
             frame.height,
             frame.stride_bytes,
             frame.len,
             frame.pixel_format,
             frame.timestamp_us,
             checksum);

    printf("first bytes :");
    const size_t preview_len = frame.len < 16 ? frame.len : 16;
    for (size_t i = 0; i < preview_len; i++) {
        printf(" %02X", frame.data[i]);
    }
    printf("\n");

    ret = bsp_camera_release_frame(camera, &frame);
    frame_active = false;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=camera step=release err=%s", esp_err_to_name(ret));
        goto out;
    }

    ESP_LOGI(TAG, "TEST PASS name=camera summary=\"frame captured len=%zu checksum=0x%08" PRIx32 "\"", frame.len, checksum);

out:
    if (frame_active) {
        (void)bsp_camera_release_frame(camera, &frame);
    }
    ret = bsp_camera_close(camera);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=camera step=close err=%s", esp_err_to_name(ret));
    }
}

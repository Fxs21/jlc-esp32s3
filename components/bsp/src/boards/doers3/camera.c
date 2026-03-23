#include "bsp_camera.h"

#include <stdlib.h>

#include "doers3_i2c.h"
#include "doers3_ioexp.h"
#include "doers3_pins.h"
#include "esp_camera.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "bsp_camera"

struct bsp_camera_s {
    camera_fb_t *frame;
    bool i2c_acquired;
    bool camera_started;
};

static const bsp_camera_desc_t s_desc = {
    .present = true,
    .width = DOERS3_CAMERA_WIDTH,
    .height = DOERS3_CAMERA_HEIGHT,
    .pixel_format = BSP_CAMERA_PIXEL_FORMAT_RGB565,
};

static esp_err_t camera_power(bool on)
{
    ESP_RETURN_ON_ERROR(doers3_ioexp_acquire(), TAG, "acquire ioexp failed");
    esp_err_t ret = doers3_ioexp_set_camera_power(on);
    esp_err_t release_ret = doers3_ioexp_release();
    if (ret == ESP_OK) {
        ret = release_ret;
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(on ? 50 : 5));
    }
    return ret;
}

static camera_config_t camera_config(void)
{
    return (camera_config_t) {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = DOERS3_CAMERA_XCLK,
        .pin_sccb_sda = -1,
        .pin_sccb_scl = -1,
        .pin_d7 = DOERS3_CAMERA_D7,
        .pin_d6 = DOERS3_CAMERA_D6,
        .pin_d5 = DOERS3_CAMERA_D5,
        .pin_d4 = DOERS3_CAMERA_D4,
        .pin_d3 = DOERS3_CAMERA_D3,
        .pin_d2 = DOERS3_CAMERA_D2,
        .pin_d1 = DOERS3_CAMERA_D1,
        .pin_d0 = DOERS3_CAMERA_D0,
        .pin_vsync = DOERS3_CAMERA_VSYNC,
        .pin_href = DOERS3_CAMERA_HREF,
        .pin_pclk = DOERS3_CAMERA_PCLK,
        .xclk_freq_hz = DOERS3_CAMERA_XCLK_FREQ_HZ,
        .ledc_timer = DOERS3_CAMERA_LEDC_TIMER,
        .ledc_channel = DOERS3_CAMERA_LEDC_CHANNEL,
        .pixel_format = PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .sccb_i2c_port = DOERS3_I2C_PORT,
    };
}

const bsp_camera_desc_t *bsp_camera_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_camera_open(bsp_camera_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    esp_err_t ret = ESP_OK;

    struct bsp_camera_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    ret = doers3_i2c_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->i2c_acquired = true;

    ret = camera_power(true);
    if (ret != ESP_OK) {
        goto err;
    }

    camera_config_t cfg = camera_config();
    ret = esp_camera_init(&cfg);
    if (ret != ESP_OK) {
        goto err;
    }
    handle->camera_started = true;

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)bsp_camera_close(handle);
    }
    return ret;
}

esp_err_t bsp_camera_close(bsp_camera_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;

    if (handle->frame != NULL) {
        esp_camera_fb_return(handle->frame);
        handle->frame = NULL;
    }
    if (handle->camera_started) {
        first_err = esp_camera_deinit();
    }
    esp_err_t power_ret = camera_power(false);
    if (power_ret != ESP_OK && first_err == ESP_OK) {
        first_err = power_ret;
    }
    if (handle->i2c_acquired) {
        esp_err_t ret = doers3_i2c_release();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }

    free(handle);
    return first_err;
}

esp_err_t bsp_camera_capture(bsp_camera_handle_t handle, bsp_camera_frame_t *out_frame)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_frame != NULL, ESP_ERR_INVALID_ARG, TAG, "out_frame is null");
    ESP_RETURN_ON_FALSE(handle->frame == NULL, ESP_ERR_INVALID_STATE, TAG, "frame not released");

    camera_fb_t *frame = esp_camera_fb_get();
    ESP_RETURN_ON_FALSE(frame != NULL, ESP_FAIL, TAG, "capture failed");

    handle->frame = frame;
    out_frame->data = frame->buf;
    out_frame->len = frame->len;
    out_frame->width = (uint16_t)frame->width;
    out_frame->height = (uint16_t)frame->height;
    out_frame->pixel_format = BSP_CAMERA_PIXEL_FORMAT_RGB565;
    out_frame->stride_bytes = (uint32_t)frame->width * 2;
    out_frame->timestamp_us = (uint64_t)esp_timer_get_time();
    return ESP_OK;
}

esp_err_t bsp_camera_release_frame(bsp_camera_handle_t handle, const bsp_camera_frame_t *frame)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(frame != NULL, ESP_ERR_INVALID_ARG, TAG, "frame is null");
    ESP_RETURN_ON_FALSE(handle->frame != NULL, ESP_ERR_INVALID_STATE, TAG, "no frame");
    ESP_RETURN_ON_FALSE(frame->data == handle->frame->buf, ESP_ERR_INVALID_ARG, TAG, "frame does not match active buffer");
    esp_camera_fb_return(handle->frame);
    handle->frame = NULL;
    return ESP_OK;
}

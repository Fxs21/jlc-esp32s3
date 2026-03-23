#include "bsp_audio.h"

#include "esp_check.h"

#define TAG "bsp_audio"

static const bsp_audio_desc_t s_desc = {
    .present = false,
    .has_playback = false,
    .has_record = false,
};

const bsp_audio_desc_t *bsp_audio_get_desc(void)
{
    return &s_desc;
}

bsp_audio_stream_cfg_t bsp_audio_default_stream_config(void)
{
    const bsp_audio_stream_cfg_t cfg = {
        .mode = BSP_AUDIO_MODE_DUPLEX,
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16,
    };
    return cfg;
}

esp_err_t bsp_audio_open(bsp_audio_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_audio_close(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

esp_err_t bsp_audio_start(bsp_audio_handle_t handle, const bsp_audio_stream_cfg_t *stream_cfg)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(stream_cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "stream_cfg is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_audio_stop(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_audio_set_out_vol(bsp_audio_handle_t handle, int volume)
{
    (void)volume;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_audio_set_in_gain(bsp_audio_handle_t handle, float gain_db)
{
    (void)gain_db;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_audio_write(bsp_audio_handle_t handle, void *data, size_t len)
{
    (void)data;
    (void)len;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_audio_read(bsp_audio_handle_t handle, void *data, size_t len)
{
    (void)data;
    (void)len;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

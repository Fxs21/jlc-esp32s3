#include "bsp_audio.h"

#include <stdlib.h>

#include "audio.h"
#include "doers3_i2c.h"
#include "doers3_ioexp.h"
#include "doers3_pins.h"
#include "esp_check.h"

#define TAG "bsp_audio"

struct bsp_audio_s {
    bsp_audio_common_t *common;
    bool i2c_acquired;
    bool ioexp_acquired;
};

static const bsp_audio_desc_t s_desc = {
    .present = true,
    .has_playback = true,
    .has_record = true,
    .supports_full_duplex = true,
    .shared_clock = true,
};

static const bsp_audio_pins_t s_pins = {
    .port = DOERS3_AUDIO_I2S_PORT,
    .mclk = DOERS3_AUDIO_I2S_MCLK,
    .bclk = DOERS3_AUDIO_I2S_BCLK,
    .ws   = DOERS3_AUDIO_I2S_WS,
    .dout = DOERS3_AUDIO_I2S_DOUT,
    .din  = DOERS3_AUDIO_I2S_DIN,
    .es8311_addr = DOERS3_AUDIO_ES8311_ADDR_7BIT,
    .es7210_addr = DOERS3_AUDIO_ES7210_ADDR_7BIT,
};

static esp_err_t pa_set(bool on)
{
    return doers3_ioexp_set_audio_pa(on);
}

const bsp_audio_desc_t *bsp_audio_get_desc(void)
{
    return &s_desc;
}

bsp_audio_config_t bsp_audio_default_config(void)
{
    return (bsp_audio_config_t) {
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .sample_format = BSP_AUDIO_SAMPLE_FORMAT_S16_LE,
    };
}

esp_err_t bsp_audio_open(const bsp_audio_config_t *config, bsp_audio_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;

    esp_err_t ret = ESP_OK;
    struct bsp_audio_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    ret = doers3_i2c_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->i2c_acquired = true;

    ret = doers3_ioexp_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->ioexp_acquired = true;
    (void)pa_set(false);

    handle->common = bsp_audio_common_create();
    ESP_GOTO_ON_FALSE(handle->common != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory");

    i2c_master_bus_handle_t bus = NULL;
    ESP_GOTO_ON_ERROR(doers3_i2c_get(&bus), err, TAG, "get i2c bus failed");
    ESP_GOTO_ON_ERROR(bsp_audio_common_init(handle->common, bus, &s_pins, config), err, TAG, "common init failed");

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)bsp_audio_close(handle);
    }
    return ret;
}

esp_err_t bsp_audio_close(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;

    if (handle->common != NULL) {
        esp_err_t ret = bsp_audio_common_record_stop(handle->common);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
        ret = bsp_audio_common_play_stop(handle->common, pa_set);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
        bsp_audio_common_destroy(handle->common);
        handle->common = NULL;
    }
    if (handle->ioexp_acquired) {
        esp_err_t ret = pa_set(false);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
        ret = doers3_ioexp_release();
        handle->ioexp_acquired = false;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (handle->i2c_acquired) {
        esp_err_t ret = doers3_i2c_release();
        handle->i2c_acquired = false;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    free(handle);
    return first_err;
}

esp_err_t bsp_audio_play_start(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_play_start(handle->common, pa_set);
}

esp_err_t bsp_audio_play_stop(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_play_stop(handle->common, pa_set);
}

esp_err_t bsp_audio_play_set_volume(bsp_audio_handle_t handle, int volume)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_play_set_volume(handle->common, volume);
}

esp_err_t bsp_audio_play_write(bsp_audio_handle_t handle, const void *data, size_t len, size_t *out_written, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_play_write(handle->common, data, len, out_written, timeout_ms);
}

esp_err_t bsp_audio_record_start(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_record_start(handle->common);
}

esp_err_t bsp_audio_record_stop(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_record_stop(handle->common);
}

esp_err_t bsp_audio_record_set_gain(bsp_audio_handle_t handle, float gain_db)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_record_set_gain(handle->common, gain_db);
}

esp_err_t bsp_audio_record_read(bsp_audio_handle_t handle, void *data, size_t len, size_t *out_read, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return bsp_audio_common_record_read(handle->common, data, len, out_read, timeout_ms);
}

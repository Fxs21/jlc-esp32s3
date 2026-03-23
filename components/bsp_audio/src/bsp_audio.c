#include <stdlib.h>

#include "bsp_audio.h"
#include "bsp_audio_es8311_es7210.h"
#include "bsp_boarddb.h"
#include "esp_check.h"

#define BSP_AUDIO_TAG "bsp_audio"

static const bsp_audio_desc_t no_audio_desc = {
    .present = false,
    .has_playback = false,
    .has_record = false,
};

struct bsp_audio_s {
    bsp_audio_driver_handle_t driver;
};

static bsp_audio_desc_t audio_desc;
static bsp_audio_es8311_es7210_cfg_t audio_cfg;
static bool audio_cfg_inited;

static bsp_audio_ctrl_pin_t ctrl_pin_from_boarddb(bsp_boarddb_ctrl_pin_t pin)
{
    bsp_audio_ctrl_pin_t out = {
        .type = (bsp_audio_ctrl_pin_type_t)pin.type,
        .active_high = pin.active_high,
    };
    if (pin.type == BSP_BOARDDB_CTRL_PIN_GPIO) {
        out.num.gpio_num = pin.num.gpio_num;
    } else {
        out.num.ioexp_num = pin.num.ioexp_num;
    }
    return out;
}

static const bsp_audio_es8311_es7210_cfg_t *audio_cfg_get(void)
{
    if (!audio_cfg_inited) {
        const bsp_boarddb_audio_desc_t *audio = &bsp_boarddb_get()->audio;
        audio_desc.present = audio->present;
        audio_desc.has_playback = audio->has_playback;
        audio_desc.has_record = audio->has_record;

        audio_cfg.i2s_port = audio->i2s_port;
        audio_cfg.mclk = audio->mclk;
        audio_cfg.bclk = audio->bclk;
        audio_cfg.ws = audio->ws;
        audio_cfg.dout = audio->dout;
        audio_cfg.din = audio->din;
        audio_cfg.es8311_addr = audio->es8311_addr;
        audio_cfg.es7210_addr = audio->es7210_addr;
        audio_cfg.es7210_mic_mask = audio->es7210_mic_mask;
        audio_cfg.pa_enable = ctrl_pin_from_boarddb(audio->pa_enable);
        audio_cfg.use_mclk = audio->use_mclk;
        audio_cfg.mclk_multiple = audio->mclk_multiple;
        audio_cfg_inited = true;
    }
    return &audio_cfg;
}

const bsp_audio_desc_t *bsp_audio_get_desc(void)
{
    const bsp_boarddb_audio_desc_t *audio = &bsp_boarddb_get()->audio;
    if (!audio->present) {
        return &no_audio_desc;
    }
    (void)audio_cfg_get();
    return &audio_desc;
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

esp_err_t bsp_audio_new(bsp_audio_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "out_handle is null");

    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    ESP_RETURN_ON_FALSE(desc->present, ESP_ERR_NOT_SUPPORTED, BSP_AUDIO_TAG, "audio not present");

    struct bsp_audio_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "no memory");

    esp_err_t ret = bsp_audio_es8311_es7210_new(audio_cfg_get(), &handle->driver);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_audio_del(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "handle is null");
    esp_err_t ret = ESP_OK;
    if (handle->driver != NULL) {
        ret = bsp_audio_es8311_es7210_del(handle->driver);
    }
    free(handle);
    return ret;
}

esp_err_t bsp_audio_start(bsp_audio_handle_t handle, const bsp_audio_stream_cfg_t *stream_cfg)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(stream_cfg != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "stream_cfg is null");

    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if ((stream_cfg->mode & BSP_AUDIO_MODE_PLAYBACK) != 0) {
        ESP_RETURN_ON_FALSE(desc->has_playback, ESP_ERR_NOT_SUPPORTED, BSP_AUDIO_TAG, "playback not present");
    }
    if ((stream_cfg->mode & BSP_AUDIO_MODE_RECORD) != 0) {
        ESP_RETURN_ON_FALSE(desc->has_record, ESP_ERR_NOT_SUPPORTED, BSP_AUDIO_TAG, "record not present");
    }

    return bsp_audio_es8311_es7210_start(handle->driver, stream_cfg);
}

esp_err_t bsp_audio_stop(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "handle is null");
    return bsp_audio_es8311_es7210_stop(handle->driver);
}

esp_err_t bsp_audio_set_out_vol(bsp_audio_handle_t handle, int volume)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "handle is null");
    return bsp_audio_es8311_es7210_set_out_vol(handle->driver, volume);
}

esp_err_t bsp_audio_set_in_gain(bsp_audio_handle_t handle, float gain_db)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "handle is null");
    return bsp_audio_es8311_es7210_set_in_gain(handle->driver, gain_db);
}

esp_err_t bsp_audio_write(bsp_audio_handle_t handle, void *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "handle is null");
    return bsp_audio_es8311_es7210_write(handle->driver, data, len);
}

esp_err_t bsp_audio_read(bsp_audio_handle_t handle, void *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "handle is null");
    return bsp_audio_es8311_es7210_read(handle->driver, data, len);
}

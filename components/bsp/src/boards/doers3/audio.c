#include "bsp_audio.h"

#include <stdlib.h>

#include "doers3_i2c.h"
#include "doers3_ioexp.h"
#include "doers3_pins.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#define TAG "bsp_audio"

struct bsp_audio_s {
    i2s_chan_handle_t tx_chan;
    i2s_chan_handle_t rx_chan;
    const audio_codec_data_if_t *data_if;
    const audio_codec_ctrl_if_t *out_ctrl_if;
    const audio_codec_ctrl_if_t *in_ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    const audio_codec_if_t *out_codec_if;
    const audio_codec_if_t *in_codec_if;
    esp_codec_dev_handle_t play_dev;
    esp_codec_dev_handle_t record_dev;
    bsp_audio_stream_cfg_t play_cfg;
    bsp_audio_stream_cfg_t record_cfg;
    bool i2c_acquired;
    bool ioexp_acquired;
    bool play_started;
    bool record_started;
    bool record_tx_clock_enabled;
};

static const bsp_audio_desc_t s_desc = {
    .present = true,
    .has_playback = true,
    .has_record = true,
};

static esp_err_t codec_to_esp(int ret)
{
    return ret == ESP_CODEC_DEV_OK ? ESP_OK : (esp_err_t)ret;
}

static esp_codec_dev_sample_info_t stream_to_sample_info(const bsp_audio_stream_cfg_t *cfg)
{
    const esp_codec_dev_sample_info_t fs = {
        .sample_rate = cfg->sample_rate,
        .channel = cfg->channels,
        .bits_per_sample = cfg->bits_per_sample,
    };
    return fs;
}

static bool stream_cfg_equal(const bsp_audio_stream_cfg_t *a, const bsp_audio_stream_cfg_t *b)
{
    return a->sample_rate == b->sample_rate &&
           a->channels == b->channels &&
           a->bits_per_sample == b->bits_per_sample;
}

static esp_err_t audio_i2s_init(struct bsp_audio_s *handle)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(DOERS3_AUDIO_I2S_PORT, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &handle->tx_chan, &handle->rx_chan), TAG, "new i2s channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = DOERS3_AUDIO_I2S_MCLK,
            .bclk = DOERS3_AUDIO_I2S_BCLK,
            .ws = DOERS3_AUDIO_I2S_WS,
            .dout = DOERS3_AUDIO_I2S_DOUT,
            .din = DOERS3_AUDIO_I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(handle->tx_chan, &std_cfg), TAG, "init i2s tx failed");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(handle->rx_chan, &std_cfg), TAG, "init i2s rx failed");
    return ESP_OK;
}

static void audio_i2s_deinit(struct bsp_audio_s *handle)
{
    if (handle->tx_chan != NULL) {
        (void)i2s_del_channel(handle->tx_chan);
        handle->tx_chan = NULL;
    }
    if (handle->rx_chan != NULL) {
        (void)i2s_del_channel(handle->rx_chan);
        handle->rx_chan = NULL;
    }
}

static esp_err_t audio_pa_set(bool enabled)
{
    const bool level = enabled ? DOERS3_IOEXP_AUDIO_PA_ENABLE_LEVEL : !DOERS3_IOEXP_AUDIO_PA_ENABLE_LEVEL;
    return doers3_ioexp_write_pin(DOERS3_IOEXP_AUDIO_PA, level);
}

const bsp_audio_desc_t *bsp_audio_get_desc(void)
{
    return &s_desc;
}

bsp_audio_stream_cfg_t bsp_audio_default_stream_config(void)
{
    const bsp_audio_stream_cfg_t cfg = {
        .sample_rate = 16000,
        .channels = 2,
        .bits_per_sample = 16,
    };
    return cfg;
}

esp_err_t bsp_audio_open(bsp_audio_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
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
    (void)audio_pa_set(false);

    ret = audio_i2s_init(handle);
    if (ret != ESP_OK) {
        goto err;
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = DOERS3_AUDIO_I2S_PORT,
        .rx_handle = handle->rx_chan,
        .tx_handle = handle->tx_chan,
    };
    handle->data_if = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_GOTO_ON_FALSE(handle->data_if != NULL, ESP_ERR_NO_MEM, err, TAG, "new i2s data failed");

    i2c_master_bus_handle_t bus = NULL;
    ret = doers3_i2c_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = DOERS3_I2C_PORT,
        .addr = DOERS3_AUDIO_ES8311_ADDR_7BIT << 1,
        .bus_handle = bus,
    };
    handle->out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_GOTO_ON_FALSE(handle->out_ctrl_if != NULL, ESP_ERR_NOT_FOUND, err, TAG, "new es8311 ctrl failed");

    i2c_cfg.addr = DOERS3_AUDIO_ES7210_ADDR_7BIT << 1;
    handle->in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_GOTO_ON_FALSE(handle->in_ctrl_if != NULL, ESP_ERR_NOT_FOUND, err, TAG, "new es7210 ctrl failed");

    handle->gpio_if = audio_codec_new_gpio();
    ESP_GOTO_ON_FALSE(handle->gpio_if != NULL, ESP_ERR_NO_MEM, err, TAG, "new gpio failed");

    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = handle->out_ctrl_if,
        .gpio_if = handle->gpio_if,
        .pa_pin = -1,
        .use_mclk = true,
    };
    handle->out_codec_if = es8311_codec_new(&es8311_cfg);
    ESP_GOTO_ON_FALSE(handle->out_codec_if != NULL, ESP_ERR_NOT_FOUND, err, TAG, "new es8311 codec failed");

    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = handle->in_ctrl_if,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4,
    };
    handle->in_codec_if = es7210_codec_new(&es7210_cfg);
    ESP_GOTO_ON_FALSE(handle->in_codec_if != NULL, ESP_ERR_NOT_FOUND, err, TAG, "new es7210 codec failed");

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = handle->out_codec_if,
        .data_if = handle->data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    handle->play_dev = esp_codec_dev_new(&dev_cfg);
    ESP_GOTO_ON_FALSE(handle->play_dev != NULL, ESP_ERR_NO_MEM, err, TAG, "new play dev failed");

    dev_cfg.codec_if = handle->in_codec_if;
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    handle->record_dev = esp_codec_dev_new(&dev_cfg);
    ESP_GOTO_ON_FALSE(handle->record_dev != NULL, ESP_ERR_NO_MEM, err, TAG, "new record dev failed");

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
    if (handle->play_started) {
        (void)bsp_audio_play_stop(handle);
    }
    if (handle->record_started) {
        (void)bsp_audio_record_stop(handle);
    }

    if (handle->play_dev != NULL) {
        esp_codec_dev_delete(handle->play_dev);
    }
    if (handle->record_dev != NULL) {
        esp_codec_dev_delete(handle->record_dev);
    }
    if (handle->in_codec_if != NULL) {
        (void)audio_codec_delete_codec_if(handle->in_codec_if);
    }
    if (handle->out_codec_if != NULL) {
        (void)audio_codec_delete_codec_if(handle->out_codec_if);
    }
    if (handle->in_ctrl_if != NULL) {
        (void)audio_codec_delete_ctrl_if(handle->in_ctrl_if);
    }
    if (handle->out_ctrl_if != NULL) {
        (void)audio_codec_delete_ctrl_if(handle->out_ctrl_if);
    }
    if (handle->gpio_if != NULL) {
        (void)audio_codec_delete_gpio_if(handle->gpio_if);
    }
    if (handle->data_if != NULL) {
        (void)audio_codec_delete_data_if(handle->data_if);
    }
    audio_i2s_deinit(handle);
    if (handle->ioexp_acquired) {
        (void)audio_pa_set(false);
        (void)doers3_ioexp_release();
    }
    if (handle->i2c_acquired) {
        (void)doers3_i2c_release();
    }
    free(handle);
    return ESP_OK;
}

esp_err_t bsp_audio_play_start(bsp_audio_handle_t handle, const bsp_audio_stream_cfg_t *stream_cfg)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(stream_cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "stream_cfg is null");
    ESP_RETURN_ON_FALSE(!handle->play_started, ESP_ERR_INVALID_STATE, TAG, "playback already started");
    ESP_RETURN_ON_FALSE(!handle->record_started || stream_cfg_equal(stream_cfg, &handle->record_cfg),
                        ESP_ERR_NOT_SUPPORTED, TAG, "playback config differs from active record");

    esp_codec_dev_sample_info_t fs = stream_to_sample_info(stream_cfg);
    ESP_RETURN_ON_ERROR(codec_to_esp(esp_codec_dev_open(handle->play_dev, &fs)), TAG, "open playback failed");
    esp_err_t ret = audio_pa_set(true);
    if (ret != ESP_OK) {
        (void)esp_codec_dev_close(handle->play_dev);
        return ret;
    }

    handle->play_cfg = *stream_cfg;
    handle->play_started = true;
    handle->record_tx_clock_enabled = false;
    return ESP_OK;
}

esp_err_t bsp_audio_play_stop(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->play_started, ESP_ERR_INVALID_STATE, TAG, "playback not started");

    esp_err_t first_err = audio_pa_set(false);
    esp_err_t ret = codec_to_esp(esp_codec_dev_close(handle->play_dev));
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    handle->play_started = false;
    if (handle->record_started) {
        handle->record_tx_clock_enabled = true;
    }
    return first_err;
}

esp_err_t bsp_audio_play_set_volume(bsp_audio_handle_t handle, int volume)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->play_started, ESP_ERR_INVALID_STATE, TAG, "playback not started");
    return codec_to_esp(esp_codec_dev_set_out_vol(handle->play_dev, volume));
}

esp_err_t bsp_audio_play_write(bsp_audio_handle_t handle, const void *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is null");
    ESP_RETURN_ON_FALSE(handle->play_started, ESP_ERR_INVALID_STATE, TAG, "playback not started");
    return codec_to_esp(esp_codec_dev_write(handle->play_dev, (void *)data, (int)len));
}

esp_err_t bsp_audio_record_start(bsp_audio_handle_t handle, const bsp_audio_stream_cfg_t *stream_cfg)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(stream_cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "stream_cfg is null");
    ESP_RETURN_ON_FALSE(!handle->record_started, ESP_ERR_INVALID_STATE, TAG, "record already started");
    ESP_RETURN_ON_FALSE(!handle->play_started || stream_cfg_equal(stream_cfg, &handle->play_cfg),
                        ESP_ERR_NOT_SUPPORTED, TAG, "record config differs from active playback");

    esp_codec_dev_sample_info_t fs = stream_to_sample_info(stream_cfg);
    ESP_RETURN_ON_ERROR(codec_to_esp(esp_codec_dev_open(handle->record_dev, &fs)), TAG, "open record failed");

    handle->record_cfg = *stream_cfg;
    handle->record_started = true;
    handle->record_tx_clock_enabled = !handle->play_started;
    return ESP_OK;
}

esp_err_t bsp_audio_record_stop(bsp_audio_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->record_started, ESP_ERR_INVALID_STATE, TAG, "record not started");

    esp_err_t first_err = codec_to_esp(esp_codec_dev_close(handle->record_dev));
    if (handle->record_tx_clock_enabled) {
        esp_err_t ret = i2s_channel_disable(handle->tx_chan);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
        handle->record_tx_clock_enabled = false;
    }

    handle->record_started = false;
    return first_err;
}

esp_err_t bsp_audio_record_set_gain(bsp_audio_handle_t handle, float gain_db)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->record_started, ESP_ERR_INVALID_STATE, TAG, "record not started");
    return codec_to_esp(esp_codec_dev_set_in_gain(handle->record_dev, gain_db));
}

esp_err_t bsp_audio_record_read(bsp_audio_handle_t handle, void *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is null");
    ESP_RETURN_ON_FALSE(handle->record_started, ESP_ERR_INVALID_STATE, TAG, "record not started");
    return codec_to_esp(esp_codec_dev_read(handle->record_dev, data, (int)len));
}

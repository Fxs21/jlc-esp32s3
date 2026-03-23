#include "audio.h"

#include <stdlib.h>

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"

#define TAG "bsp_audio_common"

struct bsp_audio_common {
    i2s_chan_handle_t tx_chan;
    i2s_chan_handle_t rx_chan;
    es8311_handle_t out_codec;
    es7210_handle_t in_codec;
    bsp_audio_config_t config;
    bool playback_started;
    bool record_started;
    bool tx_enabled;
    bool rx_enabled;
};

static TickType_t timeout_ms_to_ticks(uint32_t timeout_ms)
{
    return timeout_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
}

static esp_err_t stream_bit_width(uint8_t bits_per_sample, i2s_data_bit_width_t *out_width)
{
    if (out_width == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (bits_per_sample) {
    case 16:
        *out_width = I2S_DATA_BIT_WIDTH_16BIT;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static i2s_std_slot_config_t config_to_std_slot_config(const bsp_audio_config_t *config)
{
    i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;
    (void)stream_bit_width(config->bits_per_sample, &width);
    return (i2s_std_slot_config_t)I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(width, I2S_SLOT_MODE_STEREO);
}

static esp_err_t audio_config_validate(const bsp_audio_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->sample_rate < 8000 || config->sample_rate > 48000) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (config->sample_format != BSP_AUDIO_SAMPLE_FORMAT_S16_LE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (config->bits_per_sample != 16) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

bsp_audio_common_t *bsp_audio_common_create(void)
{
    return calloc(1, sizeof(bsp_audio_common_t));
}

void bsp_audio_common_destroy(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return;
    }
    bsp_audio_common_deinit(c);
    free(c);
}

static esp_err_t i2s_init(bsp_audio_common_t *c, const bsp_audio_pins_t *pins)
{
    const bsp_audio_config_t *config = &c->config;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(pins->port, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &c->tx_chan, &c->rx_chan), TAG, "new i2s channel failed");

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = config_to_std_slot_config(config),
        .gpio_cfg = {
            .mclk = pins->mclk,
            .bclk = pins->bclk,
            .ws = pins->ws,
            .dout = pins->dout,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(c->tx_chan, &tx_std_cfg), TAG, "init i2s tx failed");

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = config_to_std_slot_config(config),
        .gpio_cfg = {
            .mclk = pins->mclk,
            .bclk = pins->bclk,
            .ws = pins->ws,
            .dout = GPIO_NUM_NC,
            .din = pins->din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    return i2s_channel_init_std_mode(c->rx_chan, &rx_std_cfg);
}

void bsp_audio_common_deinit(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return;
    }
    if (c->rx_enabled) {
        (void)i2s_channel_disable(c->rx_chan);
        c->rx_enabled = false;
    }
    if (c->tx_enabled) {
        (void)i2s_channel_disable(c->tx_chan);
        c->tx_enabled = false;
    }
    if (c->in_codec != NULL) {
        (void)es7210_close(c->in_codec);
        c->in_codec = NULL;
    }
    if (c->out_codec != NULL) {
        (void)es8311_close(c->out_codec);
        c->out_codec = NULL;
    }
    if (c->tx_chan != NULL) {
        (void)i2s_del_channel(c->tx_chan);
        c->tx_chan = NULL;
    }
    if (c->rx_chan != NULL) {
        (void)i2s_del_channel(c->rx_chan);
        c->rx_chan = NULL;
    }
    c->playback_started = false;
    c->record_started = false;
}

esp_err_t bsp_audio_common_init(bsp_audio_common_t *c,
                                i2c_master_bus_handle_t bus,
                                const bsp_audio_pins_t *pins,
                                const bsp_audio_config_t *config)
{
    if (c == NULL || bus == NULL || pins == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(audio_config_validate(config), TAG, "bad audio config");

    c->config = *config;
    ESP_RETURN_ON_ERROR(i2s_init(c, pins), TAG, "i2s init failed");

    es8311_config_t es8311_cfg = {
        .bus = bus,
        .addr_7bit = pins->es8311_addr,
        .mode = ES8311_MODE_DAC,
        .mclk_div = 256,
        .use_mclk = true,
    };
    ESP_RETURN_ON_ERROR(es8311_open(&es8311_cfg, &c->out_codec), TAG, "open es8311 failed");
    ESP_RETURN_ON_ERROR(es8311_set_format(c->out_codec, config->sample_rate, config->bits_per_sample),
                        TAG, "set es8311 format failed");

    es7210_config_t es7210_cfg = {
        .bus = bus,
        .addr_7bit = pins->es7210_addr,
        .mic_mask = ES7210_MIC1 | ES7210_MIC2,
        .mclk_div = 256,
    };
    ESP_RETURN_ON_ERROR(es7210_open(&es7210_cfg, &c->in_codec), TAG, "open es7210 failed");
    ESP_RETURN_ON_ERROR(es7210_set_format(c->in_codec, config->sample_rate, config->bits_per_sample),
                        TAG, "set es7210 format failed");
    ESP_RETURN_ON_ERROR(es7210_mute(c->in_codec, true), TAG, "mute es7210 failed");

    return ESP_OK;
}

static esp_err_t tx_enable(bsp_audio_common_t *c)
{
    if (c->tx_enabled) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(i2s_channel_enable(c->tx_chan), TAG, "enable i2s tx failed");
    c->tx_enabled = true;
    return ESP_OK;
}

static esp_err_t tx_disable(bsp_audio_common_t *c)
{
    if (!c->tx_enabled) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(i2s_channel_disable(c->tx_chan), TAG, "disable i2s tx failed");
    c->tx_enabled = false;
    return ESP_OK;
}

static esp_err_t rx_enable(bsp_audio_common_t *c)
{
    if (c->rx_enabled) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(i2s_channel_enable(c->rx_chan), TAG, "enable i2s rx failed");
    c->rx_enabled = true;
    return ESP_OK;
}

static esp_err_t rx_disable(bsp_audio_common_t *c)
{
    if (!c->rx_enabled) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(i2s_channel_disable(c->rx_chan), TAG, "disable i2s rx failed");
    c->rx_enabled = false;
    return ESP_OK;
}

esp_err_t bsp_audio_common_play_start(bsp_audio_common_t *c, bsp_audio_pa_fn pa)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->playback_started) {
        return ESP_OK;
    }

    esp_err_t ret = tx_enable(c);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = es8311_enable(c->out_codec, true);
    if (ret != ESP_OK) {
        goto err;
    }
    ret = es8311_mute(c->out_codec, true);
    if (ret != ESP_OK) {
        goto err;
    }
    if (pa != NULL) {
        ret = pa(true);
        if (ret != ESP_OK) {
            goto err;
        }
    }
    ret = es8311_mute(c->out_codec, false);
    if (ret != ESP_OK) {
        goto err;
    }

    c->playback_started = true;
    return ESP_OK;

err:
    (void)es8311_mute(c->out_codec, true);
    if (pa != NULL) {
        (void)pa(false);
    }
    (void)es8311_enable(c->out_codec, false);
    if (!c->record_started) {
        (void)tx_disable(c);
    }
    return ret;
}

esp_err_t bsp_audio_common_play_stop(bsp_audio_common_t *c, bsp_audio_pa_fn pa)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!c->playback_started) {
        return ESP_OK;
    }

    esp_err_t first_err = es8311_mute(c->out_codec, true);
    if (pa != NULL) {
        esp_err_t ret = pa(false);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    esp_err_t ret = es8311_enable(c->out_codec, false);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }
    c->playback_started = false;
    if (!c->record_started) {
        ret = tx_disable(c);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    return first_err;
}

esp_err_t bsp_audio_common_play_set_volume(bsp_audio_common_t *c, int volume)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return es8311_set_volume(c->out_codec, volume);
}

esp_err_t bsp_audio_common_play_write(bsp_audio_common_t *c,
                                      const void *data, size_t len,
                                      size_t *out_written, uint32_t timeout_ms)
{
    if (c == NULL || data == NULL || out_written == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!c->playback_started) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t written = 0;
    ESP_RETURN_ON_ERROR(i2s_channel_write(c->tx_chan, data, len, &written, timeout_ms_to_ticks(timeout_ms)),
                        TAG, "i2s write failed");
    *out_written = written;
    return ESP_OK;
}

esp_err_t bsp_audio_common_record_start(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->record_started) {
        return ESP_OK;
    }

    esp_err_t ret = tx_enable(c);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = rx_enable(c);
    if (ret != ESP_OK) {
        goto err;
    }
    ret = es7210_enable(c->in_codec, true);
    if (ret != ESP_OK) {
        goto err;
    }
    ret = es7210_mute(c->in_codec, false);
    if (ret != ESP_OK) {
        goto err;
    }

    c->record_started = true;
    return ESP_OK;

err:
    (void)es7210_mute(c->in_codec, true);
    (void)es7210_enable(c->in_codec, false);
    (void)rx_disable(c);
    if (!c->playback_started) {
        (void)tx_disable(c);
    }
    return ret;
}

esp_err_t bsp_audio_common_record_stop(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!c->record_started) {
        return ESP_OK;
    }

    esp_err_t first_err = es7210_mute(c->in_codec, true);
    esp_err_t ret = es7210_enable(c->in_codec, false);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }
    ret = rx_disable(c);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }
    c->record_started = false;
    if (!c->playback_started) {
        ret = tx_disable(c);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    return first_err;
}

esp_err_t bsp_audio_common_record_set_gain(bsp_audio_common_t *c, float gain_db)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return es7210_set_gain(c->in_codec, gain_db);
}

esp_err_t bsp_audio_common_record_read(bsp_audio_common_t *c,
                                       void *data, size_t len,
                                       size_t *out_read, uint32_t timeout_ms)
{
    if (c == NULL || data == NULL || out_read == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!c->record_started) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t read = 0;
    ESP_RETURN_ON_ERROR(i2s_channel_read(c->rx_chan, data, len, &read, timeout_ms_to_ticks(timeout_ms)),
                        TAG, "i2s read failed");
    *out_read = read;
    return ESP_OK;
}

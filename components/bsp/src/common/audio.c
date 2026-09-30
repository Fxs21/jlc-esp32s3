#include "audio.h"

#include <stdlib.h>

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "bsp_audio"

#define AUDIO_BITS           16
#define AUDIO_TDM_SLOTS      4
#define AUDIO_DEFAULT_VOLUME 80
#define AUDIO_DEFAULT_GAIN_DB 30.0f

#define RECORD_MASK_STEREO (BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_MIC2)
#define RECORD_MASK_TDM    (BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_MIC2 | BSP_AUDIO_RECORD_CH_LOOPBACK)

// Preloaded into the TX DMA so an enabled but idle TX channel sends silence
// instead of the previous buffer content.
static const uint8_t s_tx_silence[256];

struct bsp_audio_common {
    i2s_chan_handle_t tx_chan;
    i2s_chan_handle_t rx_chan;
    es8311_handle_t out_codec;
    es7210_handle_t in_codec;
    bsp_audio_config_t config;
    SemaphoreHandle_t lock;
    uint8_t record_channels;
    bool tdm;
    bool play_started;
    bool record_started;
    bool tx_enabled;
    bool rx_enabled;
};

static bool sample_rate_supported(uint32_t sample_rate)
{
    switch (sample_rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        return true;
    default:
        return false;
    }
}

static esp_err_t config_validate(const bsp_audio_config_t *config, uint8_t *channels_out, bool *tdm_out)
{
    if (config == NULL || channels_out == NULL || tdm_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->bits_per_sample != AUDIO_BITS) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!sample_rate_supported(config->sample_rate)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    switch (config->record_channel_mask) {
    case 0:
        *channels_out = 0;
        *tdm_out = false;
        return ESP_OK;
    case RECORD_MASK_STEREO:
        *channels_out = 2;
        *tdm_out = false;
        return ESP_OK;
    case RECORD_MASK_TDM:
        *channels_out = 3;
        *tdm_out = true;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

// BSP mask 的 bit 顺序是 TDM 帧的 slot 顺序 (MIC1, LOOPBACK, MIC2), ES7210 的 mic bit
// 是按 MIC1..MIC4 排的, 所以回采 (MIC3) 和 MIC2 要换位.
static uint8_t es7210_mask_from_bsp(uint8_t bsp_mask)
{
    uint8_t codec_mask = 0;
    if ((bsp_mask & BSP_AUDIO_RECORD_CH_MIC1) != 0) {
        codec_mask |= ES7210_MIC1;
    }
    if ((bsp_mask & BSP_AUDIO_RECORD_CH_LOOPBACK) != 0) {
        codec_mask |= ES7210_MIC3;
    }
    if ((bsp_mask & BSP_AUDIO_RECORD_CH_MIC2) != 0) {
        codec_mask |= ES7210_MIC2;
    }
    return codec_mask;
}

static esp_err_t lock_acquire(bsp_audio_common_t *c)
{
    return xSemaphoreTake(c->lock, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

static void lock_release(bsp_audio_common_t *c)
{
    (void)xSemaphoreGive(c->lock);
}

static esp_err_t i2s_init_std(bsp_audio_common_t *c, const bsp_audio_pins_t *pins)
{
    i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(c->config.sample_rate),
        .slot_cfg = (i2s_std_slot_config_t)I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = pins->mclk,
            .bclk = pins->bclk,
            .ws = pins->ws,
            .dout = pins->dout,
            .din = GPIO_NUM_NC,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(c->tx_chan, &tx_cfg), TAG, "init i2s tx failed");
    if (c->rx_chan == NULL) {
        return ESP_OK;
    }

    i2s_std_config_t rx_cfg = tx_cfg;
    rx_cfg.slot_cfg = (i2s_std_slot_config_t)I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    rx_cfg.gpio_cfg.dout = GPIO_NUM_NC;
    rx_cfg.gpio_cfg.din = pins->din;
    return i2s_channel_init_std_mode(c->rx_chan, &rx_cfg);
}

static esp_err_t i2s_init_tdm(bsp_audio_common_t *c, const bsp_audio_pins_t *pins)
{
    // The record path runs in TDM because the 3-channel frame needs four
    // slots; playback shares the frame so the clock domains stay identical.
    i2s_tdm_config_t tx_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(c->config.sample_rate),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_TDM_SLOT0,
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            // 单声道样本复制到所有 slot: ES8311 是普通 I2S 从机, 只看帧的左半;
            // 只驱动 slot0 时右半是空档, 和 STD mono 的行为 (硬件复制) 不一致.
            .skip_mask = false,
            .total_slot = AUDIO_TDM_SLOTS,
        },
        .gpio_cfg = {
            .mclk = pins->mclk,
            .bclk = pins->bclk,
            .ws = pins->ws,
            .dout = pins->dout,
            .din = GPIO_NUM_NC,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_tdm_mode(c->tx_chan, &tx_cfg), TAG, "init i2s tx (tdm) failed");
    if (c->rx_chan == NULL) {
        return ESP_OK;
    }

    i2s_tdm_config_t rx_cfg = tx_cfg;
    rx_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    rx_cfg.slot_cfg.slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2;
    rx_cfg.slot_cfg.skip_mask = false;
    rx_cfg.gpio_cfg.dout = GPIO_NUM_NC;
    rx_cfg.gpio_cfg.din = pins->din;
    return i2s_channel_init_tdm_mode(c->rx_chan, &rx_cfg);
}

static esp_err_t i2s_init(bsp_audio_common_t *c, const bsp_audio_pins_t *pins)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(pins->port, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &c->tx_chan, c->record_channels != 0 ? &c->rx_chan : NULL),
                        TAG, "new i2s channel failed");
    return c->tdm ? i2s_init_tdm(c, pins) : i2s_init_std(c, pins);
}

static esp_err_t tx_enable(bsp_audio_common_t *c)
{
    if (c->tx_enabled) {
        return ESP_OK;
    }
    size_t loaded = 0;
    (void)i2s_channel_preload_data(c->tx_chan, s_tx_silence, sizeof(s_tx_silence), &loaded);
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

bsp_audio_common_t *bsp_audio_common_create(void)
{
    bsp_audio_common_t *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        return NULL;
    }
    c->lock = xSemaphoreCreateMutex();
    if (c->lock == NULL) {
        free(c);
        return NULL;
    }
    return c;
}

void bsp_audio_common_deinit(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return;
    }
    if (c->rx_enabled) {
        (void)rx_disable(c);
    }
    if (c->tx_enabled) {
        (void)tx_disable(c);
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
    c->play_started = false;
    c->record_started = false;
}

void bsp_audio_common_destroy(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return;
    }
    bsp_audio_common_deinit(c);
    if (c->lock != NULL) {
        vSemaphoreDelete(c->lock);
        c->lock = NULL;
    }
    free(c);
}

esp_err_t bsp_audio_common_init(bsp_audio_common_t *c,
                                i2c_master_bus_handle_t bus,
                                const bsp_audio_pins_t *pins,
                                const bsp_audio_config_t *config)
{
    if (c == NULL || bus == NULL || pins == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t record_channels = 0;
    bool tdm = false;
    ESP_RETURN_ON_ERROR(config_validate(config, &record_channels, &tdm), TAG, "bad audio config");
    c->config = *config;
    c->record_channels = record_channels;
    c->tdm = tdm;

    ESP_RETURN_ON_ERROR(i2s_init(c, pins), TAG, "i2s init failed");

    es8311_config_t es8311_cfg = {
        .bus = bus,
        .addr_7bit = pins->es8311_addr,
        .mode = ES8311_MODE_DAC,
        .mclk_div = 256,
        .use_mclk = true,
    };
    ESP_RETURN_ON_ERROR(es8311_open(&es8311_cfg, &c->out_codec), TAG, "open es8311 failed");
    ESP_RETURN_ON_ERROR(es8311_set_format(c->out_codec, config->sample_rate, AUDIO_BITS),
                        TAG, "es8311 format failed");
    ESP_RETURN_ON_ERROR(es8311_set_volume(c->out_codec, AUDIO_DEFAULT_VOLUME), TAG, "es8311 volume failed");
    ESP_RETURN_ON_ERROR(es8311_mute(c->out_codec, true), TAG, "es8311 mute failed");

    if (c->record_channels != 0) {
        es7210_config_t es7210_cfg = {
            .bus = bus,
            .addr_7bit = pins->es7210_addr,
            // TDM 帧固定 4 个 slot, 但 BSP 只暴露前 3 个 (MIC1, 回采, MIC2),
            // 第 4 个不开, 免得把没接的输入也录进来.
            .mic_mask = c->tdm ? (ES7210_MIC1 | ES7210_MIC2 | ES7210_MIC3)
                               : (ES7210_MIC1 | ES7210_MIC2),
        };
        ESP_RETURN_ON_ERROR(es7210_open(&es7210_cfg, &c->in_codec), TAG, "open es7210 failed");
        ESP_RETURN_ON_ERROR(es7210_set_format(c->in_codec, config->sample_rate, AUDIO_BITS),
                            TAG, "es7210 format failed");
        ESP_RETURN_ON_ERROR(es7210_set_channel_gain(c->in_codec, es7210_cfg.mic_mask, AUDIO_DEFAULT_GAIN_DB),
                            TAG, "es7210 gain failed");
        ESP_RETURN_ON_ERROR(es7210_mute(c->in_codec, true), TAG, "es7210 mute failed");
    }

    return ESP_OK;
}

esp_err_t bsp_audio_common_play_start(bsp_audio_common_t *c, bsp_audio_pa_fn pa)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_acquire(c);
    esp_err_t ret = ESP_OK;
    if (c->play_started) {
        goto out;
    }

    ret = tx_enable(c);
    if (ret != ESP_OK) {
        goto out;
    }
    ret = es8311_enable(c->out_codec, true);
    if (ret != ESP_OK) {
        goto undo_tx;
    }
    ret = es8311_mute(c->out_codec, true);
    if (ret != ESP_OK) {
        goto undo_codec;
    }
    if (pa != NULL) {
        ret = pa(true);
        if (ret != ESP_OK) {
            goto undo_codec;
        }
    }
    ret = es8311_mute(c->out_codec, false);
    if (ret != ESP_OK) {
        goto undo_pa;
    }
    c->play_started = true;
    goto out;

undo_pa:
    if (pa != NULL) {
        (void)pa(false);
    }
undo_codec:
    (void)es8311_mute(c->out_codec, true);
    (void)es8311_enable(c->out_codec, false);
undo_tx:
    if (!c->record_started) {
        (void)tx_disable(c);
    }
out:
    lock_release(c);
    return ret;
}

esp_err_t bsp_audio_common_play_stop(bsp_audio_common_t *c, bsp_audio_pa_fn pa)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_acquire(c);
    esp_err_t first_err = ESP_OK;
    if (!c->play_started) {
        goto out;
    }

    first_err = es8311_mute(c->out_codec, true);
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
    c->play_started = false;
    if (!c->record_started) {
        ret = tx_disable(c);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
out:
    lock_release(c);
    return first_err;
}

esp_err_t bsp_audio_common_play_set_volume(bsp_audio_common_t *c, int volume)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_acquire(c);
    esp_err_t ret = es8311_set_volume(c->out_codec, volume);
    lock_release(c);
    return ret;
}

esp_err_t bsp_audio_common_play_set_mute(bsp_audio_common_t *c, bool mute)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_acquire(c);
    esp_err_t ret = es8311_mute(c->out_codec, mute);
    lock_release(c);
    return ret;
}

esp_err_t bsp_audio_common_play_write(bsp_audio_common_t *c,
                                      const void *data,
                                      size_t len,
                                      size_t *written_out,
                                      uint32_t timeout_ms)
{
    if (c == NULL || data == NULL || written_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *written_out = 0;
    if (len == 0 || (len % sizeof(int16_t)) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!c->play_started) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t written = 0;
    esp_err_t ret = i2s_channel_write(c->tx_chan, data, len, &written, pdMS_TO_TICKS(timeout_ms));
    *written_out = written;
    if (ret == ESP_ERR_TIMEOUT && written > 0) {
        return ESP_OK;
    }
    return ret;
}

esp_err_t bsp_audio_common_record_start(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->record_channels == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    lock_acquire(c);
    esp_err_t ret = ESP_OK;
    if (c->record_started) {
        goto out;
    }

    // TX stays enabled as the shared clock master even without playback.
    ret = tx_enable(c);
    if (ret != ESP_OK) {
        goto out;
    }
    ret = rx_enable(c);
    if (ret != ESP_OK) {
        goto undo_tx;
    }
    ret = es7210_enable(c->in_codec, true);
    if (ret != ESP_OK) {
        goto undo_rx;
    }
    ret = es7210_mute(c->in_codec, false);
    if (ret != ESP_OK) {
        goto undo_codec;
    }
    c->record_started = true;
    goto out;

undo_codec:
    (void)es7210_mute(c->in_codec, true);
    (void)es7210_enable(c->in_codec, false);
undo_rx:
    (void)rx_disable(c);
undo_tx:
    if (!c->play_started) {
        (void)tx_disable(c);
    }
out:
    lock_release(c);
    return ret;
}

esp_err_t bsp_audio_common_record_stop(bsp_audio_common_t *c)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->record_channels == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    lock_acquire(c);
    esp_err_t first_err = ESP_OK;
    if (!c->record_started) {
        goto out;
    }

    first_err = es7210_mute(c->in_codec, true);
    esp_err_t ret = es7210_enable(c->in_codec, false);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }
    ret = rx_disable(c);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }
    c->record_started = false;
    if (!c->play_started) {
        ret = tx_disable(c);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
out:
    lock_release(c);
    return first_err;
}

esp_err_t bsp_audio_common_record_set_gain(bsp_audio_common_t *c, uint32_t channel_mask, float gain_db)
{
    if (c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->record_channels == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel_mask == 0 || (channel_mask & ~(uint32_t)c->config.record_channel_mask) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->in_codec == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    lock_acquire(c);
    esp_err_t ret = es7210_set_channel_gain(c->in_codec, es7210_mask_from_bsp((uint8_t)channel_mask), gain_db);
    lock_release(c);
    return ret;
}

esp_err_t bsp_audio_common_record_read(bsp_audio_common_t *c,
                                       void *data,
                                       size_t len,
                                       size_t *read_out,
                                       uint32_t timeout_ms)
{
    if (c == NULL || data == NULL || read_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->record_channels == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    *read_out = 0;
    const size_t frame_bytes = (size_t)c->record_channels * sizeof(int16_t);
    if (len == 0 || (len % frame_bytes) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!c->record_started) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t read = 0;
    esp_err_t ret = i2s_channel_read(c->rx_chan, data, len, &read, pdMS_TO_TICKS(timeout_ms));
    *read_out = read;
    if (ret == ESP_ERR_TIMEOUT && read > 0) {
        return ESP_OK;
    }
    return ret;
}

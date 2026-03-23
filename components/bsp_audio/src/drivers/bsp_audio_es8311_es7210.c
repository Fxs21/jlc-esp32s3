#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_audio_es8311_es7210.h"
#include "bsp_i2c_bus.h"
#include "bsp_ioexp.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#define BSP_AUDIO_TAG "bsp_audio"

typedef struct {
    i2s_chan_handle_t tx_chan;
    i2s_chan_handle_t rx_chan;
    const audio_codec_data_if_t *data_if;
    const audio_codec_ctrl_if_t *play_ctrl_if;
    const audio_codec_ctrl_if_t *record_ctrl_if;
    const audio_codec_if_t *play_codec_if;
    const audio_codec_if_t *record_codec_if;
    esp_codec_dev_handle_t play_dev;
    esp_codec_dev_handle_t record_dev;
} bsp_audio_hw_t;

typedef struct {
    bool started;
    bsp_audio_mode_t mode;
    esp_codec_dev_sample_info_t fs;
} bsp_audio_run_t;

struct bsp_audio_driver_s {
    const bsp_audio_es8311_es7210_cfg_t *cfg;
    bool bus_acquired;
    bool ioexp_acquired;
    bsp_audio_hw_t hw;
    bsp_audio_run_t run;
};

typedef struct bsp_audio_driver_s bsp_audio_es8311_es7210_ctx_t;

static inline bool mode_has_playback(bsp_audio_mode_t mode)
{
    return (mode & BSP_AUDIO_MODE_PLAYBACK) != 0;
}

static inline bool mode_has_record(bsp_audio_mode_t mode)
{
    return (mode & BSP_AUDIO_MODE_RECORD) != 0;
}

static inline bool mode_is_valid(bsp_audio_mode_t mode)
{
    return mode == BSP_AUDIO_MODE_PLAYBACK ||
           mode == BSP_AUDIO_MODE_RECORD ||
           mode == BSP_AUDIO_MODE_DUPLEX;
}

static inline void keep_first_error(esp_err_t *first_err, esp_err_t err)
{
    if (first_err != NULL && *first_err == ESP_OK && err != ESP_OK) {
        *first_err = err;
    }
}

static esp_err_t codec_dev_ret_to_esp_err(int ret)
{
    if (ret == ESP_CODEC_DEV_OK) {
        return ESP_OK;
    }
    if (ret == ESP_CODEC_DEV_WRITE_FAIL || ret == ESP_CODEC_DEV_READ_FAIL) {
        return ESP_ERR_TIMEOUT;
    }
    return (esp_err_t)ret;
}

static inline bsp_ioexp_pin_t to_ioexp_pin(bsp_audio_ctrl_pin_t pin)
{
    return (bsp_ioexp_pin_t) {
        .pin = pin.num.ioexp_num,
        .active_high = pin.active_high,
    };
}

static inline bool ctrl_pin_is_gpio(const bsp_audio_ctrl_pin_t *pin)
{
    return pin->type == BSP_AUDIO_CTRL_PIN_GPIO && pin->num.gpio_num != GPIO_NUM_NC;
}

static inline bool ctrl_pin_is_ioexp(const bsp_audio_ctrl_pin_t *pin)
{
    return pin->type == BSP_AUDIO_CTRL_PIN_IOEXP;
}

static int ctrl_pin_gpio_level(const bsp_audio_ctrl_pin_t *pin, bool asserted)
{
    return (asserted == pin->active_high) ? 1 : 0;
}

static esp_err_t ctrl_pin_init_output(const bsp_audio_ctrl_pin_t *pin, bool asserted)
{
    if (!ctrl_pin_is_gpio(pin)) {
        return ESP_OK;
    }

    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << pin->num.gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), BSP_AUDIO_TAG, "configure gpio pin failed");
    return gpio_set_level(pin->num.gpio_num, ctrl_pin_gpio_level(pin, asserted));
}

static esp_err_t ctrl_pin_set_level(const bsp_audio_ctrl_pin_t *pin, bool asserted)
{
    if (pin->type == BSP_AUDIO_CTRL_PIN_NONE) {
        return ESP_OK;
    }
    if (ctrl_pin_is_gpio(pin)) {
        return gpio_set_level(pin->num.gpio_num, ctrl_pin_gpio_level(pin, asserted));
    }
    if (ctrl_pin_is_ioexp(pin)) {
        return bsp_ioexp_set_level(to_ioexp_pin(*pin), asserted);
    }
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t codec_set_pa(bsp_audio_es8311_es7210_ctx_t *ctx, bool enabled)
{
    if (ctrl_pin_is_ioexp(&ctx->cfg->pa_enable)) {
        ESP_RETURN_ON_FALSE(ctx->ioexp_acquired, ESP_ERR_INVALID_STATE, BSP_AUDIO_TAG, "ioexp not acquired");
    }
    return ctrl_pin_set_level(&ctx->cfg->pa_enable, enabled);
}

static esp_err_t audio_init_i2s(bsp_audio_es8311_es7210_ctx_t *ctx)
{
    bsp_audio_hw_t *hw = &ctx->hw;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(ctx->cfg->i2s_port, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &hw->tx_chan, &hw->rx_chan), BSP_AUDIO_TAG, "create i2s channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = ctx->cfg->mclk,
            .bclk = ctx->cfg->bclk,
            .ws = ctx->cfg->ws,
            .dout = ctx->cfg->dout,
            .din = ctx->cfg->din,
        },
    };

    if (ctx->cfg->use_mclk) {
        std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    }

    esp_err_t ret = i2s_channel_init_std_mode(hw->tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = i2s_channel_init_std_mode(hw->rx_chan, &std_cfg);
    if (ret != ESP_OK) {
        goto err;
    }

    return ESP_OK;

err:
    if (hw->tx_chan != NULL) {
        (void)i2s_del_channel(hw->tx_chan);
        hw->tx_chan = NULL;
    }
    if (hw->rx_chan != NULL) {
        (void)i2s_del_channel(hw->rx_chan);
        hw->rx_chan = NULL;
    }
    return ret;
}

static void audio_deinit_i2s(bsp_audio_es8311_es7210_ctx_t *ctx)
{
    bsp_audio_hw_t *hw = &ctx->hw;

    if (hw->tx_chan != NULL) {
        (void)i2s_channel_disable(hw->tx_chan);
        (void)i2s_del_channel(hw->tx_chan);
        hw->tx_chan = NULL;
    }

    if (hw->rx_chan != NULL) {
        (void)i2s_channel_disable(hw->rx_chan);
        (void)i2s_del_channel(hw->rx_chan);
        hw->rx_chan = NULL;
    }
}

static esp_err_t codec_create_data_if(bsp_audio_es8311_es7210_ctx_t *ctx)
{
    bsp_audio_hw_t *hw = &ctx->hw;
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = hw->rx_chan,
        .tx_handle = hw->tx_chan,
    };

    hw->data_if = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(hw->data_if != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "new i2s data if failed");
    return ESP_OK;
}

static esp_err_t codec_create_playback_dev(bsp_audio_es8311_es7210_ctx_t *ctx, i2c_master_bus_handle_t i2c_bus)
{
    bsp_audio_hw_t *hw = &ctx->hw;

    audio_codec_i2c_cfg_t i2c_cfg = {
        .bus_handle = i2c_bus,
        .addr = ctx->cfg->es8311_addr,
    };
    hw->play_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(hw->play_ctrl_if != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "new es8311 ctrl if failed");

    es8311_codec_cfg_t codec_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = hw->play_ctrl_if,
        .gpio_if = NULL,
        .pa_pin = -1,
        .use_mclk = ctx->cfg->use_mclk,
        .mclk_div = ctx->cfg->mclk_multiple,
    };
    hw->play_codec_if = es8311_codec_new(&codec_cfg);
    ESP_RETURN_ON_FALSE(hw->play_codec_if != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "new es8311 codec if failed");

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = hw->play_codec_if,
        .data_if = hw->data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    hw->play_dev = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(hw->play_dev != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "new play dev failed");
    return ESP_OK;
}

static esp_err_t codec_create_record_dev(bsp_audio_es8311_es7210_ctx_t *ctx, i2c_master_bus_handle_t i2c_bus)
{
    bsp_audio_hw_t *hw = &ctx->hw;

    audio_codec_i2c_cfg_t i2c_cfg = {
        .bus_handle = i2c_bus,
        .addr = ctx->cfg->es7210_addr,
    };
    hw->record_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(hw->record_ctrl_if != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "new es7210 ctrl if failed");

    es7210_codec_cfg_t codec_cfg = {
        .ctrl_if = hw->record_ctrl_if,
        .mic_selected = ctx->cfg->es7210_mic_mask,
        .mclk_div = ctx->cfg->mclk_multiple,
    };
    hw->record_codec_if = es7210_codec_new(&codec_cfg);
    ESP_RETURN_ON_FALSE(hw->record_codec_if != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "new es7210 codec if failed");

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = hw->record_codec_if,
        .data_if = hw->data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
    };
    hw->record_dev = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(hw->record_dev != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "new record dev failed");
    return ESP_OK;
}

static void codec_destroy_dev_stack(bsp_audio_es8311_es7210_ctx_t *ctx)
{
    bsp_audio_hw_t *hw = &ctx->hw;

    if (hw->play_dev != NULL) {
        esp_codec_dev_delete(hw->play_dev);
        hw->play_dev = NULL;
    }
    if (hw->record_dev != NULL) {
        esp_codec_dev_delete(hw->record_dev);
        hw->record_dev = NULL;
    }

    if (hw->play_codec_if != NULL) {
        (void)audio_codec_delete_codec_if(hw->play_codec_if);
        hw->play_codec_if = NULL;
    }
    if (hw->record_codec_if != NULL) {
        (void)audio_codec_delete_codec_if(hw->record_codec_if);
        hw->record_codec_if = NULL;
    }

    if (hw->play_ctrl_if != NULL) {
        (void)audio_codec_delete_ctrl_if(hw->play_ctrl_if);
        hw->play_ctrl_if = NULL;
    }
    if (hw->record_ctrl_if != NULL) {
        (void)audio_codec_delete_ctrl_if(hw->record_ctrl_if);
        hw->record_ctrl_if = NULL;
    }

    if (hw->data_if != NULL) {
        (void)audio_codec_delete_data_if(hw->data_if);
        hw->data_if = NULL;
    }
}

static esp_err_t make_fs(const bsp_audio_stream_cfg_t *stream_cfg,
                         uint16_t mclk_multiple,
                         esp_codec_dev_sample_info_t *out_fs)
{
    ESP_RETURN_ON_FALSE(stream_cfg != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "stream_cfg is null");
    ESP_RETURN_ON_FALSE(out_fs != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "out_fs is null");
    ESP_RETURN_ON_FALSE(mode_is_valid(stream_cfg->mode), ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "invalid mode");
    ESP_RETURN_ON_FALSE(stream_cfg->sample_rate > 0, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "sample_rate must be > 0");
    ESP_RETURN_ON_FALSE(stream_cfg->channels == 1 || stream_cfg->channels == 2,
                        ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "channels must be 1 or 2");
    ESP_RETURN_ON_FALSE(stream_cfg->bits_per_sample == 16 ||
                        stream_cfg->bits_per_sample == 24 ||
                        stream_cfg->bits_per_sample == 32,
                        ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "bits_per_sample must be 16/24/32");

    out_fs->sample_rate = stream_cfg->sample_rate;
    out_fs->channel = stream_cfg->channels;
    out_fs->bits_per_sample = stream_cfg->bits_per_sample;
    out_fs->channel_mask = 0;
    out_fs->mclk_multiple = mclk_multiple;
    return ESP_OK;
}

esp_err_t bsp_audio_es8311_es7210_new(const bsp_audio_es8311_es7210_cfg_t *cfg,
                                      bsp_audio_driver_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "out_handle is null");

    bsp_audio_es8311_es7210_ctx_t *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_NO_MEM, BSP_AUDIO_TAG, "no memory");
    ctx->cfg = cfg;
    ctx->run.mode = BSP_AUDIO_MODE_NONE;

    bsp_i2c_bus_cfg_t bus_cfg = bsp_i2c_bus_default_cfg();
    esp_err_t ret = bsp_i2c_bus_open(&bus_cfg);
    if (ret != ESP_OK) {
        free(ctx);
        return ret;
    }
    ctx->bus_acquired = true;

    i2c_master_bus_handle_t i2c_bus = NULL;
    ret = bsp_i2c_bus_get(&i2c_bus);
    if (ret != ESP_OK) {
        (void)bsp_audio_es8311_es7210_del(ctx);
        return ret;
    }

    if (ctrl_pin_is_gpio(&cfg->pa_enable)) {
        ret = ctrl_pin_init_output(&cfg->pa_enable, false);
        if (ret != ESP_OK) {
            (void)bsp_audio_es8311_es7210_del(ctx);
            return ret;
        }
    }

    if (ctrl_pin_is_ioexp(&cfg->pa_enable)) {
        ret = bsp_ioexp_open();
        if (ret != ESP_OK) {
            (void)bsp_audio_es8311_es7210_del(ctx);
            return ret;
        }
        ctx->ioexp_acquired = true;
    }

    ret = audio_init_i2s(ctx);
    if (ret == ESP_OK) {
        ret = codec_create_data_if(ctx);
    }
    if (ret == ESP_OK) {
        ret = codec_create_playback_dev(ctx, i2c_bus);
    }
    if (ret == ESP_OK) {
        ret = codec_create_record_dev(ctx, i2c_bus);
    }
    if (ret != ESP_OK) {
        (void)bsp_audio_es8311_es7210_del(ctx);
        return ret;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t bsp_audio_es8311_es7210_start(bsp_audio_driver_handle_t handle,
                                        const bsp_audio_stream_cfg_t *stream_cfg)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "ctx is null");
    ESP_RETURN_ON_FALSE(stream_cfg != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "stream_cfg is null");

    bsp_audio_es8311_es7210_ctx_t *ctx = handle;
    ESP_RETURN_ON_FALSE(!ctx->run.started, ESP_ERR_INVALID_STATE, BSP_AUDIO_TAG, "codec already started");

    esp_codec_dev_sample_info_t fs = { 0 };
    ESP_RETURN_ON_ERROR(make_fs(stream_cfg, ctx->cfg->mclk_multiple, &fs), BSP_AUDIO_TAG, "invalid stream config");

    esp_err_t ret = ESP_OK;
    bsp_audio_mode_t opened_mode = BSP_AUDIO_MODE_NONE;

    if (mode_has_playback(stream_cfg->mode)) {
        ret = codec_dev_ret_to_esp_err(esp_codec_dev_open(ctx->hw.play_dev, &fs));
        if (ret != ESP_OK) {
            goto err;
        }
        opened_mode = (bsp_audio_mode_t)(opened_mode | BSP_AUDIO_MODE_PLAYBACK);

        ret = codec_set_pa(ctx, true);
        if (ret != ESP_OK) {
            goto err;
        }
    }

    if (mode_has_record(stream_cfg->mode)) {
        ret = codec_dev_ret_to_esp_err(esp_codec_dev_open(ctx->hw.record_dev, &fs));
        if (ret != ESP_OK) {
            goto err;
        }
        opened_mode = (bsp_audio_mode_t)(opened_mode | BSP_AUDIO_MODE_RECORD);
    }

    ctx->run.started = true;
    ctx->run.mode = stream_cfg->mode;
    ctx->run.fs = fs;
    return ESP_OK;

err:
    if (mode_has_record(opened_mode)) {
        (void)esp_codec_dev_close(ctx->hw.record_dev);
    }
    if (mode_has_playback(opened_mode)) {
        (void)codec_set_pa(ctx, false);
        (void)esp_codec_dev_close(ctx->hw.play_dev);
    }
    return ret;
}

esp_err_t bsp_audio_es8311_es7210_stop(bsp_audio_driver_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "ctx is null");

    bsp_audio_es8311_es7210_ctx_t *ctx = handle;
    if (!ctx->run.started) {
        return ESP_OK;
    }

    esp_err_t first_err = ESP_OK;

    if (mode_has_playback(ctx->run.mode)) {
        keep_first_error(&first_err, codec_set_pa(ctx, false));
        keep_first_error(&first_err, codec_dev_ret_to_esp_err(esp_codec_dev_close(ctx->hw.play_dev)));
    }
    if (mode_has_record(ctx->run.mode)) {
        keep_first_error(&first_err, codec_dev_ret_to_esp_err(esp_codec_dev_close(ctx->hw.record_dev)));
    }

    if (first_err == ESP_OK) {
        ctx->run.started = false;
        ctx->run.mode = BSP_AUDIO_MODE_NONE;
        memset(&ctx->run.fs, 0, sizeof(ctx->run.fs));
    }
    return first_err;
}

esp_err_t bsp_audio_es8311_es7210_del(bsp_audio_driver_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "ctx is null");

    bsp_audio_es8311_es7210_ctx_t *ctx = handle;
    esp_err_t first_err = bsp_audio_es8311_es7210_stop(ctx);
    codec_destroy_dev_stack(ctx);
    audio_deinit_i2s(ctx);

    if (ctx->ioexp_acquired) {
        keep_first_error(&first_err, bsp_ioexp_close());
        ctx->ioexp_acquired = false;
    }
    if (ctx->bus_acquired) {
        keep_first_error(&first_err, bsp_i2c_bus_close());
        ctx->bus_acquired = false;
    }

    free(ctx);
    return first_err;
}

esp_err_t bsp_audio_es8311_es7210_set_out_vol(bsp_audio_driver_handle_t handle, int volume)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "ctx is null");

    bsp_audio_es8311_es7210_ctx_t *ctx = handle;
    ESP_RETURN_ON_FALSE(ctx->run.started && mode_has_playback(ctx->run.mode),
                        ESP_ERR_INVALID_STATE, BSP_AUDIO_TAG, "playback not started");
    return codec_dev_ret_to_esp_err(esp_codec_dev_set_out_vol(ctx->hw.play_dev, volume));
}

esp_err_t bsp_audio_es8311_es7210_set_in_gain(bsp_audio_driver_handle_t handle, float gain_db)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "ctx is null");

    bsp_audio_es8311_es7210_ctx_t *ctx = handle;
    ESP_RETURN_ON_FALSE(ctx->run.started && mode_has_record(ctx->run.mode),
                        ESP_ERR_INVALID_STATE, BSP_AUDIO_TAG, "record not started");
    return codec_dev_ret_to_esp_err(esp_codec_dev_set_in_gain(ctx->hw.record_dev, gain_db));
}

esp_err_t bsp_audio_es8311_es7210_write(bsp_audio_driver_handle_t handle, void *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "ctx is null");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "data is null");
    ESP_RETURN_ON_FALSE(len > 0 && len <= (size_t)INT_MAX, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "invalid len");

    bsp_audio_es8311_es7210_ctx_t *ctx = handle;
    ESP_RETURN_ON_FALSE(ctx->run.started && mode_has_playback(ctx->run.mode),
                        ESP_ERR_INVALID_STATE, BSP_AUDIO_TAG, "playback not started");
    return codec_dev_ret_to_esp_err(esp_codec_dev_write(ctx->hw.play_dev, data, (int)len));
}

esp_err_t bsp_audio_es8311_es7210_read(bsp_audio_driver_handle_t handle, void *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "ctx is null");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "data is null");
    ESP_RETURN_ON_FALSE(len > 0 && len <= (size_t)INT_MAX, ESP_ERR_INVALID_ARG, BSP_AUDIO_TAG, "invalid len");

    bsp_audio_es8311_es7210_ctx_t *ctx = handle;
    ESP_RETURN_ON_FALSE(ctx->run.started && mode_has_record(ctx->run.mode),
                        ESP_ERR_INVALID_STATE, BSP_AUDIO_TAG, "record not started");
    return codec_dev_ret_to_esp_err(esp_codec_dev_read(ctx->hw.record_dev, data, (int)len));
}

#include "es7210.h"

#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"

#define TAG "ES7210"
#define ES7210_I2C_SPEED_HZ 100000
#define ES7210_I2C_TIMEOUT_MS 100
#define ES7210_DEFAULT_MCLK_DIV 256
#define ES7210_TDM_MIC_THRESHOLD 3
#define ES7210_MAX_REGISTER 0x4e

#define ES7210_RESET_REG00           0x00
#define ES7210_CLOCK_OFF_REG01       0x01
#define ES7210_MAINCLK_REG02         0x02
#define ES7210_MASTER_CLK_REG03      0x03
#define ES7210_LRCK_DIVH_REG04       0x04
#define ES7210_LRCK_DIVL_REG05       0x05
#define ES7210_POWER_DOWN_REG06      0x06
#define ES7210_OSR_REG07             0x07
#define ES7210_MODE_CONFIG_REG08     0x08
#define ES7210_TIME_CONTROL0_REG09   0x09
#define ES7210_TIME_CONTROL1_REG0A   0x0A
#define ES7210_SDP_INTERFACE1_REG11  0x11
#define ES7210_SDP_INTERFACE2_REG12  0x12
#define ES7210_ADC34_MUTERANGE_REG14 0x14
#define ES7210_ADC12_MUTERANGE_REG15 0x15
#define ES7210_ADC34_HPF2_REG20      0x20
#define ES7210_ADC34_HPF1_REG21      0x21
#define ES7210_ADC12_HPF1_REG22      0x22
#define ES7210_ADC12_HPF2_REG23      0x23
#define ES7210_ANALOG_REG40          0x40
#define ES7210_MIC12_BIAS_REG41      0x41
#define ES7210_MIC34_BIAS_REG42      0x42
#define ES7210_MIC1_GAIN_REG43       0x43
#define ES7210_MIC2_GAIN_REG44       0x44
#define ES7210_MIC3_GAIN_REG45       0x45
#define ES7210_MIC4_GAIN_REG46       0x46
#define ES7210_MIC1_POWER_REG47      0x47
#define ES7210_MIC2_POWER_REG48      0x48
#define ES7210_MIC3_POWER_REG49      0x49
#define ES7210_MIC4_POWER_REG4A      0x4A
#define ES7210_MIC12_POWER_REG4B     0x4B
#define ES7210_MIC34_POWER_REG4C     0x4C

struct es7210_coeff {
    uint32_t mclk;
    uint32_t lrck;
    uint8_t adc_div;
    uint8_t dll;
    uint8_t doubler;
    uint8_t osr;
    uint8_t lrck_h;
    uint8_t lrck_l;
};

static const struct es7210_coeff s_coeffs[] = {
    {12288000, 8000, 0x03, 0x01, 0x00, 0x20, 0x06, 0x00},
    {16384000, 8000, 0x04, 0x01, 0x00, 0x20, 0x08, 0x00},
    {19200000, 8000, 0x1e, 0x00, 0x01, 0x28, 0x09, 0x60},
    {4096000, 8000, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00},
    {11289600, 11025, 0x02, 0x01, 0x00, 0x20, 0x01, 0x00},
    {12288000, 12000, 0x02, 0x01, 0x00, 0x20, 0x04, 0x00},
    {19200000, 12000, 0x14, 0x00, 0x01, 0x28, 0x06, 0x40},
    {4096000, 16000, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
    {19200000, 16000, 0x0a, 0x00, 0x00, 0x1e, 0x04, 0x80},
    {16384000, 16000, 0x02, 0x01, 0x00, 0x20, 0x04, 0x00},
    {12288000, 16000, 0x03, 0x01, 0x01, 0x20, 0x03, 0x00},
    {11289600, 22050, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00},
    {12288000, 24000, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00},
    {19200000, 24000, 0x0a, 0x00, 0x01, 0x28, 0x03, 0x20},
    {8192000, 32000, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
    {12288000, 32000, 0x03, 0x00, 0x00, 0x20, 0x01, 0x80},
    {16384000, 32000, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00},
    {19200000, 32000, 0x05, 0x00, 0x00, 0x1e, 0x02, 0x58},
    {11289600, 44100, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
    {12288000, 48000, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
    {19200000, 48000, 0x05, 0x00, 0x01, 0x28, 0x01, 0x90},
    {16384000, 64000, 0x01, 0x01, 0x00, 0x20, 0x01, 0x00},
    {19200000, 64000, 0x05, 0x00, 0x01, 0x1e, 0x01, 0x2c},
    {11289600, 88200, 0x01, 0x01, 0x01, 0x20, 0x00, 0x80},
    {12288000, 96000, 0x01, 0x01, 0x01, 0x20, 0x00, 0x80},
    {19200000, 96000, 0x05, 0x00, 0x01, 0x28, 0x00, 0xc8},
};

struct es7210_s {
    i2c_master_dev_handle_t dev;
    uint8_t mic_mask;
    uint8_t gain;
    uint8_t off_reg;
    uint16_t mclk_div;
    bool master_mode;
    es7210_mclk_src_t mclk_src;
    bool opened;
    bool enabled;
};

static esp_err_t es7210_write_reg(es7210_handle_t handle, uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = { reg, value };
    return i2c_master_transmit(handle->dev, payload, sizeof(payload), ES7210_I2C_TIMEOUT_MS);
}

static esp_err_t es7210_read_reg(es7210_handle_t handle, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(handle->dev, &reg, sizeof(reg), value, sizeof(*value), ES7210_I2C_TIMEOUT_MS);
}

static esp_err_t es7210_update_reg(es7210_handle_t handle, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t old_value = 0;
    ESP_RETURN_ON_ERROR(es7210_read_reg(handle, reg, &old_value), TAG, "read reg failed");
    return es7210_write_reg(handle, reg, (old_value & (uint8_t)~mask) | (value & mask));
}

static const struct es7210_coeff *es7210_find_coeff(es7210_handle_t handle, uint32_t sample_rate)
{
    const uint32_t mclk = sample_rate * handle->mclk_div;
    for (size_t i = 0; i < sizeof(s_coeffs) / sizeof(s_coeffs[0]); ++i) {
        if (s_coeffs[i].lrck == sample_rate && s_coeffs[i].mclk == mclk) {
            return &s_coeffs[i];
        }
    }
    return NULL;
}

static uint8_t es7210_mic_count(es7210_handle_t handle)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if ((handle->mic_mask & (uint8_t)(1u << i)) != 0) {
            count++;
        }
    }
    return count;
}

static bool es7210_is_tdm_mode(es7210_handle_t handle)
{
    return es7210_mic_count(handle) >= ES7210_TDM_MIC_THRESHOLD;
}

static uint8_t es7210_gain_to_reg(float gain_db)
{
    gain_db += 0.5f;
    if (gain_db < 3.0f) {
        return 0;
    }
    if (gain_db < 33.0f) {
        return (uint8_t)(gain_db / 3.0f);
    }
    if (gain_db < 34.5f) {
        return 10;
    }
    if (gain_db < 36.0f) {
        return 12;
    }
    if (gain_db < 37.0f) {
        return 13;
    }
    return 14;
}

static uint8_t es7210_gain_reg_for_mic(uint8_t mic_index)
{
    return (uint8_t)(ES7210_MIC1_GAIN_REG43 + mic_index);
}

static esp_err_t es7210_set_selected_mics(es7210_handle_t handle)
{
    ESP_RETURN_ON_FALSE((handle->mic_mask & (ES7210_MIC1 | ES7210_MIC2 | ES7210_MIC3 | ES7210_MIC4)) != 0,
                        ESP_ERR_INVALID_ARG, TAG, "bad mic mask");

    for (uint8_t i = 0; i < 4; ++i) {
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, es7210_gain_reg_for_mic(i), 0x10, 0x00),
                            TAG, "disable mic gain failed");
    }
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC12_POWER_REG4B, 0xff), TAG, "mic12 off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC34_POWER_REG4C, 0xff), TAG, "mic34 off failed");

    if ((handle->mic_mask & ES7210_MIC1) != 0) {
        ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC1");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_CLOCK_OFF_REG01, 0x0b, 0x00), TAG, "mic1 clock failed");
        ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC12_POWER_REG4B, 0x00), TAG, "mic1 power failed");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_MIC1_GAIN_REG43, 0x1f, (uint8_t)(0x10 | handle->gain)), TAG, "mic1 gain failed");
    }
    if ((handle->mic_mask & ES7210_MIC2) != 0) {
        ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC2");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_CLOCK_OFF_REG01, 0x0b, 0x00), TAG, "mic2 clock failed");
        ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC12_POWER_REG4B, 0x00), TAG, "mic2 power failed");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_MIC2_GAIN_REG44, 0x1f, (uint8_t)(0x10 | handle->gain)), TAG, "mic2 gain failed");
    }
    if ((handle->mic_mask & ES7210_MIC3) != 0) {
        ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC3");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_CLOCK_OFF_REG01, 0x15, 0x00), TAG, "mic3 clock failed");
        ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC34_POWER_REG4C, 0x00), TAG, "mic3 power failed");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_MIC3_GAIN_REG45, 0x1f, (uint8_t)(0x10 | handle->gain)), TAG, "mic3 gain failed");
    }
    if ((handle->mic_mask & ES7210_MIC4) != 0) {
        ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC4");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_CLOCK_OFF_REG01, 0x15, 0x00), TAG, "mic4 clock failed");
        ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC34_POWER_REG4C, 0x00), TAG, "mic4 power failed");
        ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_MIC4_GAIN_REG46, 0x1f, (uint8_t)(0x10 | handle->gain)), TAG, "mic4 gain failed");
    }

    if (es7210_is_tdm_mode(handle)) {
        ESP_LOGI(TAG, "Enable TDM mode");
        return es7210_write_reg(handle, ES7210_SDP_INTERFACE2_REG12, 0x02);
    }
    return es7210_write_reg(handle, ES7210_SDP_INTERFACE2_REG12, 0x00);
}

static esp_err_t es7210_set_bits(es7210_handle_t handle, uint8_t bits_per_sample)
{
    uint8_t bits = 0;
    switch (bits_per_sample) {
    case 8:
    case 16:
        bits = 0x60;
        break;
    case 24:
        bits = 0x00;
        break;
    case 32:
        bits = 0x80;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_LOGI(TAG, "Bits %d", bits_per_sample);
    return es7210_update_reg(handle, ES7210_SDP_INTERFACE1_REG11, 0xe0, bits);
}

static esp_err_t es7210_set_i2s_format(es7210_handle_t handle)
{
    return es7210_update_reg(handle, ES7210_SDP_INTERFACE1_REG11, 0x03, 0x00);
}

static esp_err_t es7210_config_sample(es7210_handle_t handle, uint32_t sample_rate)
{
    if (!handle->master_mode) {
        return ESP_OK;
    }
    const struct es7210_coeff *coeff = es7210_find_coeff(handle, sample_rate);
    ESP_RETURN_ON_FALSE(coeff != NULL, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported sample rate");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MAINCLK_REG02,
                                         (uint8_t)(coeff->adc_div | (coeff->doubler << 6) | (coeff->dll << 7))),
                        TAG, "main clock failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_OSR_REG07, coeff->osr), TAG, "osr failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_LRCK_DIVH_REG04, coeff->lrck_h), TAG, "lrck h failed");
    return es7210_write_reg(handle, ES7210_LRCK_DIVL_REG05, coeff->lrck_l);
}

static esp_err_t es7210_start(es7210_handle_t handle)
{
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_CLOCK_OFF_REG01, handle->off_reg), TAG, "clock on failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_POWER_DOWN_REG06, 0x00), TAG, "power on failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_ANALOG_REG40, 0x43), TAG, "analog failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC1_POWER_REG47, 0x08), TAG, "mic1 power failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC2_POWER_REG48, 0x08), TAG, "mic2 power failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC3_POWER_REG49, 0x08), TAG, "mic3 power failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC4_POWER_REG4A, 0x08), TAG, "mic4 power failed");
    ESP_RETURN_ON_ERROR(es7210_set_selected_mics(handle), TAG, "select mics failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_ANALOG_REG40, 0x43), TAG, "analog failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_RESET_REG00, 0x71), TAG, "reset failed");
    return es7210_write_reg(handle, ES7210_RESET_REG00, 0x41);
}

static esp_err_t es7210_stop(es7210_handle_t handle)
{
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC1_POWER_REG47, 0xff), TAG, "mic1 off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC2_POWER_REG48, 0xff), TAG, "mic2 off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC3_POWER_REG49, 0xff), TAG, "mic3 off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC4_POWER_REG4A, 0xff), TAG, "mic4 off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC12_POWER_REG4B, 0xff), TAG, "mic12 off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_MIC34_POWER_REG4C, 0xff), TAG, "mic34 off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_ANALOG_REG40, 0xc0), TAG, "analog off failed");
    ESP_RETURN_ON_ERROR(es7210_write_reg(handle, ES7210_CLOCK_OFF_REG01, 0x7f), TAG, "clock off failed");
    return es7210_write_reg(handle, ES7210_POWER_DOWN_REG06, 0x07);
}

esp_err_t es7210_open(const es7210_config_t *cfg, es7210_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    struct es7210_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->mic_mask = cfg->mic_mask != 0 ? cfg->mic_mask : (ES7210_MIC1 | ES7210_MIC2);
    handle->gain = es7210_gain_to_reg(30.0f);
    handle->mclk_div = cfg->mclk_div != 0 ? cfg->mclk_div : ES7210_DEFAULT_MCLK_DIV;
    handle->master_mode = cfg->master_mode;
    handle->mclk_src = cfg->mclk_src;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->addr_7bit,
        .scl_speed_hz = ES7210_I2C_SPEED_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(cfg->bus, &dev_cfg, &handle->dev);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_RESET_REG00, 0xff), err, TAG, "reset failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_RESET_REG00, 0x41), err, TAG, "reset failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_CLOCK_OFF_REG01, 0x3f), err, TAG, "clock off failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_TIME_CONTROL0_REG09, 0x30), err, TAG, "time0 failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_TIME_CONTROL1_REG0A, 0x30), err, TAG, "time1 failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_ADC12_HPF2_REG23, 0x2a), err, TAG, "hpf failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_ADC12_HPF1_REG22, 0x0a), err, TAG, "hpf failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_ADC34_HPF2_REG20, 0x0a), err, TAG, "hpf failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_ADC34_HPF1_REG21, 0x2a), err, TAG, "hpf failed");
    ESP_GOTO_ON_ERROR(es7210_update_reg(handle, ES7210_MODE_CONFIG_REG08, 0x01, handle->master_mode ? 0x01 : 0x00),
                      err, TAG, "work mode failed");
    if (handle->master_mode) {
        ESP_GOTO_ON_ERROR(es7210_update_reg(handle, ES7210_MASTER_CLK_REG03, 0x80,
                                            handle->mclk_src == ES7210_MCLK_FROM_CLOCK_DOUBLER ? 0x80 : 0x00),
                          err, TAG, "mclk source failed");
    }
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_ANALOG_REG40, 0x43), err, TAG, "analog failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_MIC12_BIAS_REG41, 0x70), err, TAG, "bias failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_MIC34_BIAS_REG42, 0x70), err, TAG, "bias failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_OSR_REG07, 0x20), err, TAG, "osr failed");
    ESP_GOTO_ON_ERROR(es7210_write_reg(handle, ES7210_MAINCLK_REG02, 0xc1), err, TAG, "main clock failed");
    ESP_GOTO_ON_ERROR(es7210_set_selected_mics(handle), err, TAG, "select mics failed");
    ESP_GOTO_ON_ERROR(es7210_set_gain(handle, 30.0f), err, TAG, "gain failed");
    ESP_GOTO_ON_ERROR(es7210_read_reg(handle, ES7210_CLOCK_OFF_REG01, &handle->off_reg), err, TAG, "read off reg failed");

    ESP_LOGI(TAG, "Work in %s mode", handle->master_mode ? "Master" : "Slave");
    handle->opened = true;
    *out_handle = handle;
    return ESP_OK;

err:
    (void)i2c_master_bus_rm_device(handle->dev);
    free(handle);
    return ret;
}

esp_err_t es7210_close(es7210_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    if (handle->opened && handle->enabled) {
        (void)es7210_stop(handle);
    }
    if (handle->dev != NULL) {
        (void)i2c_master_bus_rm_device(handle->dev);
    }
    free(handle);
    return ESP_OK;
}

esp_err_t es7210_set_format(es7210_handle_t handle, uint32_t sample_rate, uint8_t bits_per_sample)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->opened, ESP_ERR_INVALID_STATE, TAG, "not opened");
    ESP_RETURN_ON_ERROR(es7210_set_bits(handle, bits_per_sample), TAG, "set bits failed");
    ESP_RETURN_ON_ERROR(es7210_config_sample(handle, sample_rate), TAG, "set sample failed");
    return es7210_set_i2s_format(handle);
}

esp_err_t es7210_enable(es7210_handle_t handle, bool enable)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->opened, ESP_ERR_INVALID_STATE, TAG, "not opened");
    if (handle->enabled == enable) {
        return ESP_OK;
    }
    esp_err_t ret = enable ? es7210_start(handle) : es7210_stop(handle);
    if (ret == ESP_OK) {
        handle->enabled = enable;
    }
    return ret;
}

esp_err_t es7210_mute(es7210_handle_t handle, bool mute)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    uint8_t value = mute ? 0x03 : 0x00;
    ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_ADC34_MUTERANGE_REG14, 0x03, value), TAG, "mute adc34 failed");
    ESP_RETURN_ON_ERROR(es7210_update_reg(handle, ES7210_ADC12_MUTERANGE_REG15, 0x03, value), TAG, "mute adc12 failed");
    ESP_LOGI(TAG, "%s", mute ? "Muted" : "Unmuted");
    return ESP_OK;
}

esp_err_t es7210_set_mic_mask(es7210_handle_t handle, uint8_t mic_mask)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE((mic_mask & (ES7210_MIC1 | ES7210_MIC2 | ES7210_MIC3 | ES7210_MIC4)) != 0,
                        ESP_ERR_INVALID_ARG, TAG, "bad mic mask");
    handle->mic_mask = mic_mask;
    ESP_RETURN_ON_ERROR(es7210_set_selected_mics(handle), TAG, "select mics failed");
    return es7210_read_reg(handle, ES7210_CLOCK_OFF_REG01, &handle->off_reg);
}

esp_err_t es7210_set_gain(es7210_handle_t handle, float gain_db)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    handle->gain = es7210_gain_to_reg(gain_db);
    return es7210_set_channel_gain(handle, handle->mic_mask, gain_db);
}

esp_err_t es7210_set_channel_gain(es7210_handle_t handle, uint8_t channel_mask, float gain_db)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    uint8_t gain = es7210_gain_to_reg(gain_db);
    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t mic = (uint8_t)(1u << i);
        if ((handle->mic_mask & mic) != 0 && (channel_mask & mic) != 0) {
            ESP_RETURN_ON_ERROR(es7210_update_reg(handle, es7210_gain_reg_for_mic(i), 0x0f, gain), TAG,
                                "mic gain failed");
        }
    }
    return ESP_OK;
}

esp_err_t es7210_read_register(es7210_handle_t handle, uint8_t reg, uint8_t *value)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(value != NULL, ESP_ERR_INVALID_ARG, TAG, "value is null");
    return es7210_read_reg(handle, reg, value);
}

esp_err_t es7210_write_register(es7210_handle_t handle, uint8_t reg, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return es7210_write_reg(handle, reg, value);
}

void es7210_dump_registers(es7210_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    for (uint8_t reg = 0; reg <= ES7210_MAX_REGISTER; ++reg) {
        uint8_t value = 0;
        if (es7210_read_reg(handle, reg, &value) != ESP_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", reg, value);
    }
}

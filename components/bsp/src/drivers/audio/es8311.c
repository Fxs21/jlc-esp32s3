#include "es8311.h"

#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"

#define TAG "ES8311"
#define ES8311_I2C_SPEED_HZ 100000
#define ES8311_I2C_TIMEOUT_MS 100
#define ES8311_DEFAULT_MCLK_DIV 256
#define ES8311_MAX_REGISTER 0xff

#define ES8311_RESET_REG00       0x00
#define ES8311_CLK_MANAGER_REG01 0x01
#define ES8311_CLK_MANAGER_REG02 0x02
#define ES8311_CLK_MANAGER_REG03 0x03
#define ES8311_CLK_MANAGER_REG04 0x04
#define ES8311_CLK_MANAGER_REG05 0x05
#define ES8311_CLK_MANAGER_REG06 0x06
#define ES8311_CLK_MANAGER_REG07 0x07
#define ES8311_CLK_MANAGER_REG08 0x08
#define ES8311_SDPIN_REG09       0x09
#define ES8311_SDPOUT_REG0A      0x0A
#define ES8311_SYSTEM_REG0B      0x0B
#define ES8311_SYSTEM_REG0C      0x0C
#define ES8311_SYSTEM_REG0D      0x0D
#define ES8311_SYSTEM_REG0E      0x0E
#define ES8311_SYSTEM_REG10      0x10
#define ES8311_SYSTEM_REG11      0x11
#define ES8311_SYSTEM_REG12      0x12
#define ES8311_SYSTEM_REG13      0x13
#define ES8311_SYSTEM_REG14      0x14
#define ES8311_ADC_REG15         0x15
#define ES8311_ADC_REG16         0x16
#define ES8311_ADC_REG17         0x17
#define ES8311_ADC_REG1B         0x1B
#define ES8311_ADC_REG1C         0x1C
#define ES8311_DAC_REG31         0x31
#define ES8311_DAC_REG32         0x32
#define ES8311_DAC_REG37         0x37
#define ES8311_GPIO_REG44        0x44
#define ES8311_GP_REG45          0x45

struct es8311_coeff {
    uint32_t mclk;
    uint32_t rate;
    uint8_t pre_div;
    uint8_t pre_multi;
    uint8_t adc_div;
    uint8_t dac_div;
    uint8_t fs_mode;
    uint8_t lrck_h;
    uint8_t lrck_l;
    uint8_t bclk_div;
    uint8_t adc_osr;
    uint8_t dac_osr;
};

static const struct es8311_coeff s_coeffs[] = {
    {12288000, 8000, 0x06, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {18432000, 8000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x05, 0xff, 0x18, 0x10, 0x20},
    {16384000, 8000, 0x08, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {8192000, 8000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000, 8000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {4096000, 8000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2048000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000, 8000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1024000, 8000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {11289600, 11025, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {5644800, 11025, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2822400, 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1411200, 11025, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {12288000, 12000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000, 12000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000, 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000, 12000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {12288000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {18432000, 16000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x20},
    {16384000, 16000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {8192000, 16000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000, 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {4096000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000, 16000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2048000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000, 16000, 0x03, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1024000, 16000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {11289600, 22050, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 22050, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 24000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 24000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 24000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 32000, 0x03, 0x04, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 32000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 32000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 32000, 0x03, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 32000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 32000, 0x03, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {1024000, 32000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {11289600, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 44100, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 44100, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 48000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 48000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 64000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 64000, 0x03, 0x04, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {16384000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 64000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 64000, 0x01, 0x04, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {4096000, 64000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 64000, 0x01, 0x08, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {2048000, 64000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 64000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0xbf, 0x03, 0x18, 0x18},
    {1024000, 64000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {11289600, 88200, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 88200, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 88200, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 88200, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {24576000, 96000, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 96000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 96000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 96000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 96000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 96000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
};

struct es8311_s {
    i2c_master_dev_handle_t dev;
    uint8_t mode;
    uint16_t mclk_div;
    bool master_mode;
    bool use_mclk;
    bool digital_mic;
    bool invert_mclk;
    bool invert_sclk;
    bool no_dac_ref;
    bool opened;
    bool enabled;
};

static esp_err_t es8311_write_reg(es8311_handle_t handle, uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = { reg, value };
    return i2c_master_transmit(handle->dev, payload, sizeof(payload), ES8311_I2C_TIMEOUT_MS);
}

static esp_err_t es8311_read_reg(es8311_handle_t handle, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(handle->dev, &reg, sizeof(reg), value, sizeof(*value), ES8311_I2C_TIMEOUT_MS);
}

static esp_err_t es8311_update_reg(es8311_handle_t handle, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t old_value = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(handle, reg, &old_value), TAG, "read reg failed");
    return es8311_write_reg(handle, reg, (old_value & (uint8_t)~mask) | (value & mask));
}

static const struct es8311_coeff *es8311_find_coeff(es8311_handle_t handle, uint32_t sample_rate)
{
    const uint32_t mclk = sample_rate * handle->mclk_div;
    for (size_t i = 0; i < sizeof(s_coeffs) / sizeof(s_coeffs[0]); ++i) {
        if (s_coeffs[i].rate == sample_rate && s_coeffs[i].mclk == mclk) {
            return &s_coeffs[i];
        }
    }
    return NULL;
}

static uint8_t es8311_multi_to_reg(uint8_t pre_multi)
{
    switch (pre_multi) {
    case 1:
        return 0;
    case 2:
        return 1;
    case 4:
        return 2;
    case 8:
        return 3;
    default:
        return 0xff;
    }
}

static esp_err_t es8311_set_bits(es8311_handle_t handle, uint8_t bits_per_sample)
{
    uint8_t bits = 0;
    switch (bits_per_sample) {
    case 16:
        bits = 0x0c;
        break;
    case 24:
        bits = 0x00;
        break;
    case 32:
        bits = 0x10;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_RETURN_ON_ERROR(es8311_update_reg(handle, ES8311_SDPIN_REG09, 0x1c, bits), TAG, "set dac bits failed");
    return es8311_update_reg(handle, ES8311_SDPOUT_REG0A, 0x1c, bits);
}

static esp_err_t es8311_set_i2s_format(es8311_handle_t handle)
{
    ESP_RETURN_ON_ERROR(es8311_update_reg(handle, ES8311_SDPIN_REG09, 0x03, 0x00), TAG, "set dac format failed");
    return es8311_update_reg(handle, ES8311_SDPOUT_REG0A, 0x03, 0x00);
}

static esp_err_t es8311_config_sample(es8311_handle_t handle, uint32_t sample_rate)
{
    const struct es8311_coeff *coeff = es8311_find_coeff(handle, sample_rate);
    ESP_RETURN_ON_FALSE(coeff != NULL, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported sample rate");

    uint8_t multi = es8311_multi_to_reg(coeff->pre_multi);
    ESP_RETURN_ON_FALSE(multi != 0xff, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported clock multiplier");
    if (!handle->use_mclk) {
        multi = (sample_rate == 8000) ? 2 : 3;
    }

    uint8_t regv = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(handle, ES8311_CLK_MANAGER_REG02, &regv), TAG, "read clk2 failed");
    regv &= 0x07;
    regv |= (uint8_t)((coeff->pre_div - 1) << 5);
    regv |= (uint8_t)(multi << 3);
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG02, regv), TAG, "write clk2 failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG05,
                                         (uint8_t)(((coeff->adc_div - 1) << 4) | (coeff->dac_div - 1))),
                        TAG, "write clk5 failed");
    ESP_RETURN_ON_ERROR(es8311_update_reg(handle, ES8311_CLK_MANAGER_REG03, 0x7f,
                                          (uint8_t)((coeff->fs_mode << 6) | coeff->adc_osr)),
                        TAG, "write clk3 failed");
    ESP_RETURN_ON_ERROR(es8311_update_reg(handle, ES8311_CLK_MANAGER_REG04, 0x7f, coeff->dac_osr),
                        TAG, "write clk4 failed");
    ESP_RETURN_ON_ERROR(es8311_update_reg(handle, ES8311_CLK_MANAGER_REG07, 0x3f, coeff->lrck_h),
                        TAG, "write clk7 failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG08, coeff->lrck_l), TAG, "write clk8 failed");

    uint8_t bclk = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(handle, ES8311_CLK_MANAGER_REG06, &bclk), TAG, "read clk6 failed");
    bclk &= 0xe0;
    bclk |= (coeff->bclk_div < 19) ? (uint8_t)(coeff->bclk_div - 1) : coeff->bclk_div;
    return es8311_write_reg(handle, ES8311_CLK_MANAGER_REG06, bclk);
}

static esp_err_t es8311_start(es8311_handle_t handle)
{
    uint8_t regv = handle->master_mode ? 0xc0 : 0x80;
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_RESET_REG00, regv), TAG, "reset failed");

    regv = 0x3f;
    if (!handle->use_mclk) {
        regv |= 0x80;
    }
    if (handle->invert_mclk) {
        regv |= 0x40;
    } else {
        regv &= (uint8_t)~0x40;
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG01, regv), TAG, "clk1 failed");

    uint8_t dac_iface = 0;
    uint8_t adc_iface = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(handle, ES8311_SDPIN_REG09, &dac_iface), TAG, "read sdp in failed");
    ESP_RETURN_ON_ERROR(es8311_read_reg(handle, ES8311_SDPOUT_REG0A, &adc_iface), TAG, "read sdp out failed");
    if ((handle->mode & ES8311_MODE_DAC) != 0) {
        dac_iface &= (uint8_t)~0x40;
    }
    if ((handle->mode & ES8311_MODE_ADC) != 0) {
        adc_iface &= (uint8_t)~0x40;
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SDPIN_REG09, dac_iface), TAG, "write sdp in failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SDPOUT_REG0A, adc_iface), TAG, "write sdp out failed");

    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_ADC_REG17, 0xbf), TAG, "adc vol failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0E, 0x02), TAG, "system 0e failed");
    if ((handle->mode & ES8311_MODE_DAC) != 0) {
        ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG12, 0x00), TAG, "system 12 failed");
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG14, 0x1a), TAG, "system 14 failed");
    ESP_RETURN_ON_ERROR(es8311_update_reg(handle, ES8311_SYSTEM_REG14, 0x40, handle->digital_mic ? 0x40 : 0x00),
                        TAG, "digital mic failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0D, 0x01), TAG, "system 0d failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_ADC_REG15, 0x40), TAG, "adc 15 failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_DAC_REG37, 0x08), TAG, "dac 37 failed");
    return es8311_write_reg(handle, ES8311_GP_REG45, 0x00);
}

static esp_err_t es8311_suspend(es8311_handle_t handle)
{
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_DAC_REG32, 0x00), TAG, "dac vol off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_ADC_REG17, 0x00), TAG, "adc vol off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0E, 0xff), TAG, "system 0e off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG12, 0x02), TAG, "system 12 off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG14, 0x00), TAG, "system 14 off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0D, 0xfa), TAG, "system 0d off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_ADC_REG15, 0x00), TAG, "adc off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG02, 0x10), TAG, "clk2 off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_RESET_REG00, 0x00), TAG, "reset off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_RESET_REG00, 0x1f), TAG, "reset off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG01, 0x30), TAG, "clk1 off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG01, 0x00), TAG, "clk1 off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_GP_REG45, 0x00), TAG, "gp off failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0D, 0xfc), TAG, "system 0d off failed");
    return es8311_write_reg(handle, ES8311_CLK_MANAGER_REG02, 0x00);
}

esp_err_t es8311_open(const es8311_config_t *cfg, es8311_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    struct es8311_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->mode = cfg->mode != 0 ? cfg->mode : ES8311_MODE_DAC;
    handle->mclk_div = cfg->mclk_div != 0 ? cfg->mclk_div : ES8311_DEFAULT_MCLK_DIV;
    handle->master_mode = cfg->master_mode;
    handle->use_mclk = cfg->use_mclk;
    handle->digital_mic = cfg->digital_mic;
    handle->invert_mclk = cfg->invert_mclk;
    handle->invert_sclk = cfg->invert_sclk;
    handle->no_dac_ref = cfg->no_dac_ref;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->addr_7bit,
        .scl_speed_hz = ES8311_I2C_SPEED_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(cfg->bus, &dev_cfg, &handle->dev);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0D, 0xfa), err, TAG, "system 0d failed");

    /* ES8311 can NACK the first GPIO_REG44 write; retry once like the official codec driver. */
    esp_err_t gpio_ret = es8311_write_reg(handle, ES8311_GPIO_REG44, 0x08);
    if (gpio_ret != ESP_OK) {
        ESP_LOGW(TAG, "gpio 44 first write failed, retrying: %s", esp_err_to_name(gpio_ret));
    }
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_GPIO_REG44, 0x08), err, TAG, "gpio 44 retry failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG01, 0x30), err, TAG, "clk1 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG02, 0x00), err, TAG, "clk2 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG03, 0x10), err, TAG, "clk3 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_ADC_REG16, 0x24), err, TAG, "adc gain failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG04, 0x10), err, TAG, "clk4 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG05, 0x00), err, TAG, "clk5 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0B, 0x00), err, TAG, "system 0b failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG0C, 0x00), err, TAG, "system 0c failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG10, 0x1f), err, TAG, "system 10 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG11, 0x7f), err, TAG, "system 11 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_RESET_REG00, 0x80), err, TAG, "reset failed");
    ESP_GOTO_ON_ERROR(es8311_update_reg(handle, ES8311_RESET_REG00, 0x40, handle->master_mode ? 0x40 : 0x00), err,
                      TAG, "work mode failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_CLK_MANAGER_REG01,
                                       (uint8_t)((handle->use_mclk ? 0x3f : 0xbf) |
                                                 (handle->invert_mclk ? 0x40 : 0x00))),
                      err, TAG, "clk source failed");
    ESP_GOTO_ON_ERROR(es8311_update_reg(handle, ES8311_CLK_MANAGER_REG06, 0x20, handle->invert_sclk ? 0x20 : 0x00),
                      err, TAG, "sclk failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_SYSTEM_REG13, 0x10), err, TAG, "system 13 failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_ADC_REG1B, 0x0a), err, TAG, "adc 1b failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_ADC_REG1C, 0x6a), err, TAG, "adc 1c failed");
    ESP_GOTO_ON_ERROR(es8311_write_reg(handle, ES8311_GPIO_REG44, handle->no_dac_ref ? 0x08 : 0x58), err,
                      TAG, "dac ref failed");

    ESP_LOGI(TAG, "Work in %s mode", handle->master_mode ? "Master" : "Slave");
    handle->opened = true;
    *out_handle = handle;
    return ESP_OK;

err:
    (void)i2c_master_bus_rm_device(handle->dev);
    free(handle);
    return ret;
}

esp_err_t es8311_close(es8311_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    if (handle->opened) {
        (void)es8311_mute(handle, true);
        (void)es8311_suspend(handle);
    }
    if (handle->dev != NULL) {
        (void)i2c_master_bus_rm_device(handle->dev);
    }
    free(handle);
    return ESP_OK;
}

esp_err_t es8311_set_format(es8311_handle_t handle, uint32_t sample_rate, uint8_t bits_per_sample)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->opened, ESP_ERR_INVALID_STATE, TAG, "not opened");
    ESP_RETURN_ON_ERROR(es8311_set_bits(handle, bits_per_sample), TAG, "set bits failed");
    ESP_RETURN_ON_ERROR(es8311_set_i2s_format(handle), TAG, "set format failed");
    return es8311_config_sample(handle, sample_rate);
}

esp_err_t es8311_enable(es8311_handle_t handle, bool enable)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->opened, ESP_ERR_INVALID_STATE, TAG, "not opened");
    if (handle->enabled == enable) {
        return ESP_OK;
    }
    esp_err_t ret = enable ? es8311_start(handle) : es8311_suspend(handle);
    if (ret == ESP_OK) {
        handle->enabled = enable;
    }
    return ret;
}

esp_err_t es8311_mute(es8311_handle_t handle, bool mute)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    uint8_t regv = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(handle, ES8311_DAC_REG31, &regv), TAG, "read mute failed");
    regv &= 0x9f;
    if (mute) {
        regv |= 0x60;
    }
    return es8311_write_reg(handle, ES8311_DAC_REG31, regv);
}

esp_err_t es8311_set_volume(es8311_handle_t handle, int volume)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }
    const int min_db_x2 = -191;
    const int max_db_x2 = 64;
    int target_db_x2 = volume <= 0 ? min_db_x2 : (volume - 100);
    uint8_t regv = (uint8_t)(((target_db_x2 - min_db_x2) * 255) / (max_db_x2 - min_db_x2));
    return es8311_write_reg(handle, ES8311_DAC_REG32, regv);
}

esp_err_t es8311_set_mic_gain(es8311_handle_t handle, float gain_db)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    uint8_t regv = 0x00;
    if (gain_db < 6.0f) {
        regv = 0x00;
    } else if (gain_db < 12.0f) {
        regv = 0x01;
    } else if (gain_db < 18.0f) {
        regv = 0x02;
    } else if (gain_db < 24.0f) {
        regv = 0x03;
    } else if (gain_db < 30.0f) {
        regv = 0x04;
    } else if (gain_db < 36.0f) {
        regv = 0x05;
    } else if (gain_db < 42.0f) {
        regv = 0x06;
    } else {
        regv = 0x07;
    }
    return es8311_write_reg(handle, ES8311_ADC_REG16, regv);
}

esp_err_t es8311_read_register(es8311_handle_t handle, uint8_t reg, uint8_t *value)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(value != NULL, ESP_ERR_INVALID_ARG, TAG, "value is null");
    return es8311_read_reg(handle, reg, value);
}

esp_err_t es8311_write_register(es8311_handle_t handle, uint8_t reg, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return es8311_write_reg(handle, reg, value);
}

void es8311_dump_registers(es8311_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    for (uint16_t reg = 0; reg < ES8311_MAX_REGISTER; ++reg) {
        uint8_t value = 0;
        if (es8311_read_reg(handle, (uint8_t)reg, &value) != ESP_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", reg, value);
    }
}

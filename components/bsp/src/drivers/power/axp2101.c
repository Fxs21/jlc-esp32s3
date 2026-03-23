#include "axp2101.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"

#define TAG "axp2101"

#define AXP2101_I2C_TIMEOUT_MS 1000

#define AXP2101_CHIP_ID 0x4A

#define AXP2101_STATUS1 0x00
#define AXP2101_STATUS2 0x01
#define AXP2101_IC_TYPE 0x03
#define AXP2101_ADC_CHANNEL_CTRL 0x30
#define AXP2101_ADC_DATA0 0x34
#define AXP2101_INTEN2 0x41
#define AXP2101_INTEN3 0x42
#define AXP2101_INTSTS1 0x48
#define AXP2101_INTSTS2 0x49
#define AXP2101_INTSTS3 0x4A
#define AXP2101_TS_PIN_CTRL 0x50
#define AXP2101_BAT_PERCENT_DATA 0xA4

#define AXP2101_INT2_VBUS_INSERT 0x80
#define AXP2101_INT2_VBUS_REMOVE 0x40
#define AXP2101_INT2_BAT_INSERT 0x20
#define AXP2101_INT2_BAT_REMOVE 0x10
#define AXP2101_INT2_PKEY_SHORT 0x08
#define AXP2101_INT2_PKEY_LONG 0x04
#define AXP2101_INT3_CHARGE_DONE 0x10
#define AXP2101_INT3_CHARGE_START 0x08

#define AXP2101_ADC_BATT_EN BIT0
#define AXP2101_ADC_TS_EN BIT1
#define AXP2101_ADC_VBUS_EN BIT2
#define AXP2101_ADC_SYS_EN BIT3
#define AXP2101_ADC_TEMP_EN BIT4

struct axp2101_t {
    i2c_master_dev_handle_t dev;
};

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out_value)
{
    return i2c_master_transmit_receive(dev, &reg, sizeof(reg), out_value, sizeof(*out_value), AXP2101_I2C_TIMEOUT_MS);
}

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    const uint8_t payload[2] = {reg, value};
    return i2c_master_transmit(dev, payload, sizeof(payload), AXP2101_I2C_TIMEOUT_MS);
}

static esp_err_t update_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t old_value = 0;
    ESP_RETURN_ON_ERROR(read_reg(dev, reg, &old_value), TAG, "read reg 0x%02x failed", reg);
    const uint8_t new_value = (old_value & (uint8_t)~mask) | (value & mask);
    return write_reg(dev, reg, new_value);
}

static esp_err_t read_h6l8(i2c_master_dev_handle_t dev, uint8_t high_reg, uint16_t *out_value)
{
    uint8_t buf[2] = {0};
    ESP_RETURN_ON_ERROR(read_reg(dev, high_reg, &buf[0]), TAG, "read high reg failed");
    ESP_RETURN_ON_ERROR(read_reg(dev, high_reg + 1, &buf[1]), TAG, "read low reg failed");
    *out_value = (uint16_t)(((buf[0] & 0x3F) << 8) | buf[1]);
    return ESP_OK;
}

static esp_err_t read_h5l8(i2c_master_dev_handle_t dev, uint8_t high_reg, uint16_t *out_value)
{
    uint8_t buf[2] = {0};
    ESP_RETURN_ON_ERROR(read_reg(dev, high_reg, &buf[0]), TAG, "read high reg failed");
    ESP_RETURN_ON_ERROR(read_reg(dev, high_reg + 1, &buf[1]), TAG, "read low reg failed");
    *out_value = (uint16_t)(((buf[0] & 0x1F) << 8) | buf[1]);
    return ESP_OK;
}

static float convert_temp_c(uint16_t raw)
{
    return 22.0f + (7274.0f - (float)raw) / 20.0f;
}

esp_err_t axp2101_open(const axp2101_config_t *cfg, axp2101_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->dev != NULL, ESP_ERR_INVALID_ARG, TAG, "dev is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    uint8_t chip_id = 0;
    ESP_RETURN_ON_ERROR(read_reg(cfg->dev, AXP2101_IC_TYPE, &chip_id), TAG, "read chip id failed");
    ESP_RETURN_ON_FALSE(chip_id == AXP2101_CHIP_ID, ESP_ERR_NOT_FOUND, TAG, "unexpected chip id 0x%02x", chip_id);

    struct axp2101_t *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->dev = cfg->dev;
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t axp2101_close(axp2101_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    free(handle);
    return ESP_OK;
}

esp_err_t axp2101_enable_adc(axp2101_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    const uint8_t mask = AXP2101_ADC_BATT_EN | AXP2101_ADC_VBUS_EN | AXP2101_ADC_SYS_EN | AXP2101_ADC_TEMP_EN;
    return update_reg(handle->dev, AXP2101_ADC_CHANNEL_CTRL, mask, mask);
}

esp_err_t axp2101_disable_ts_adc(axp2101_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return update_reg(handle->dev, AXP2101_ADC_CHANNEL_CTRL, AXP2101_ADC_TS_EN, 0);
}

esp_err_t axp2101_enable_default_irqs(axp2101_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_ERROR(update_reg(handle->dev, AXP2101_INTEN2, 0xFC, 0xFC), TAG, "enable int2 failed");
    ESP_RETURN_ON_ERROR(update_reg(handle->dev, AXP2101_INTEN3, AXP2101_INT3_CHARGE_DONE | AXP2101_INT3_CHARGE_START,
                                   AXP2101_INT3_CHARGE_DONE | AXP2101_INT3_CHARGE_START), TAG, "enable int3 failed");
    for (uint8_t reg = AXP2101_INTSTS1; reg <= AXP2101_INTSTS3; ++reg) {
        ESP_RETURN_ON_ERROR(write_reg(handle->dev, reg, 0xFF), TAG, "clear irq failed");
    }
    return ESP_OK;
}

esp_err_t axp2101_get_status(axp2101_handle_t handle, axp2101_status_t *out_status)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_status != NULL, ESP_ERR_INVALID_ARG, TAG, "out_status is null");

    uint8_t status1 = 0;
    uint8_t status2 = 0;
    ESP_RETURN_ON_ERROR(read_reg(handle->dev, AXP2101_STATUS1, &status1), TAG, "read status1 failed");
    ESP_RETURN_ON_ERROR(read_reg(handle->dev, AXP2101_STATUS2, &status2), TAG, "read status2 failed");

    axp2101_status_t st = {0};
    st.vbus_good = (status1 & BIT5) != 0;
    st.vbus_present = st.vbus_good;
    st.battery_present = (status1 & BIT3) != 0;

    const uint8_t power_state = status2 >> 5;
    st.charging = power_state == 0x01;
    st.discharging = power_state == 0x02;
    st.standby = power_state == 0x00;
    st.charge_state = (axp2101_charge_state_t)(status2 & 0x07);

    st.battery_percent = -1;
    st.battery_voltage_mv = -1;
    st.vbus_voltage_mv = -1;
    st.system_voltage_mv = -1;

    uint16_t raw = 0;
    uint8_t percent = 0;
    ESP_RETURN_ON_ERROR(read_reg(handle->dev, AXP2101_BAT_PERCENT_DATA, &percent), TAG, "read battery percent failed");
    if (st.battery_present) {
        ESP_RETURN_ON_ERROR(read_h5l8(handle->dev, AXP2101_ADC_DATA0, &raw), TAG, "read battery voltage failed");
        st.battery_voltage_mv = raw;
        st.battery_percent = percent <= 100 ? percent : -1;
    }

    if (st.vbus_present) {
        ESP_RETURN_ON_ERROR(read_h6l8(handle->dev, AXP2101_ADC_DATA0 + 4, &raw), TAG, "read vbus voltage failed");
        st.vbus_voltage_mv = raw;
    }

    ESP_RETURN_ON_ERROR(read_h6l8(handle->dev, AXP2101_ADC_DATA0 + 6, &raw), TAG, "read system voltage failed");
    st.system_voltage_mv = raw;

    ESP_RETURN_ON_ERROR(read_h6l8(handle->dev, AXP2101_ADC_DATA0 + 8, &raw), TAG, "read temperature failed");
    st.temperature_c = convert_temp_c(raw);

    *out_status = st;
    return ESP_OK;
}

esp_err_t axp2101_get_events(axp2101_handle_t handle, axp2101_event_t *out_events, bool clear)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_events != NULL, ESP_ERR_INVALID_ARG, TAG, "out_events is null");

    uint8_t int2 = 0;
    uint8_t int3 = 0;
    ESP_RETURN_ON_ERROR(read_reg(handle->dev, AXP2101_INTSTS2, &int2), TAG, "read int2 failed");
    ESP_RETURN_ON_ERROR(read_reg(handle->dev, AXP2101_INTSTS3, &int3), TAG, "read int3 failed");

    axp2101_event_t events = AXP2101_EVENT_NONE;
    if (int2 & AXP2101_INT2_VBUS_INSERT) {
        events |= AXP2101_EVENT_VBUS_INSERT;
    }
    if (int2 & AXP2101_INT2_VBUS_REMOVE) {
        events |= AXP2101_EVENT_VBUS_REMOVE;
    }
    if (int2 & AXP2101_INT2_BAT_INSERT) {
        events |= AXP2101_EVENT_BATTERY_INSERT;
    }
    if (int2 & AXP2101_INT2_BAT_REMOVE) {
        events |= AXP2101_EVENT_BATTERY_REMOVE;
    }
    if (int2 & AXP2101_INT2_PKEY_SHORT) {
        events |= AXP2101_EVENT_POWER_KEY_SHORT;
    }
    if (int2 & AXP2101_INT2_PKEY_LONG) {
        events |= AXP2101_EVENT_POWER_KEY_LONG;
    }
    if (int3 & AXP2101_INT3_CHARGE_START) {
        events |= AXP2101_EVENT_CHARGE_START;
    }
    if (int3 & AXP2101_INT3_CHARGE_DONE) {
        events |= AXP2101_EVENT_CHARGE_DONE;
    }

    if (clear) {
        ESP_RETURN_ON_ERROR(write_reg(handle->dev, AXP2101_INTSTS1, 0xFF), TAG, "clear int1 failed");
        ESP_RETURN_ON_ERROR(write_reg(handle->dev, AXP2101_INTSTS2, 0xFF), TAG, "clear int2 failed");
        ESP_RETURN_ON_ERROR(write_reg(handle->dev, AXP2101_INTSTS3, 0xFF), TAG, "clear int3 failed");
    }

    *out_events = events;
    return ESP_OK;
}

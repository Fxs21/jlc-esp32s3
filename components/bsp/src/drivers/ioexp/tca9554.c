#include "tca9554.h"

#include <stdlib.h>

#include "esp_check.h"

#define TAG "tca9554"
#define TCA9554_INPUT_PORT_REG        0x00
#define TCA9554_OUTPUT_PORT_REG       0x01
#define TCA9554_POLARITY_INV_PORT_REG 0x02
#define TCA9554_CONFIG_PORT_REG       0x03
#define TCA9554_TIMEOUT_MS            1000

struct tca9554_s {
    i2c_master_dev_handle_t dev;
    uint8_t output_shadow;
};

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = { reg, value };
    return i2c_master_transmit(dev, payload, sizeof(payload), TCA9554_TIMEOUT_MS);
}

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(dev, &reg, sizeof(reg), value, sizeof(*value), TCA9554_TIMEOUT_MS);
}

esp_err_t tca9554_open(const tca9554_config_t *cfg, tca9554_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->dev != NULL, ESP_ERR_INVALID_ARG, TAG, "dev is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    struct tca9554_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    esp_err_t ret = write_reg(cfg->dev, TCA9554_OUTPUT_PORT_REG, cfg->output_default);
    if (ret == ESP_OK) {
        ret = write_reg(cfg->dev, TCA9554_CONFIG_PORT_REG, cfg->direction_mask);
    }
    if (ret == ESP_OK) {
        ret = write_reg(cfg->dev, TCA9554_POLARITY_INV_PORT_REG, 0x00);
    }
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    handle->dev = cfg->dev;
    handle->output_shadow = cfg->output_default;
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t tca9554_close(tca9554_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    free(handle);
    return ESP_OK;
}

esp_err_t tca9554_read_port(tca9554_handle_t handle, uint8_t *value)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(value != NULL, ESP_ERR_INVALID_ARG, TAG, "value is null");
    return read_reg(handle->dev, TCA9554_INPUT_PORT_REG, value);
}

esp_err_t tca9554_write_port(tca9554_handle_t handle, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t ret = write_reg(handle->dev, TCA9554_OUTPUT_PORT_REG, value);
    if (ret == ESP_OK) {
        handle->output_shadow = value;
    }
    return ret;
}

esp_err_t tca9554_write_port_masked(tca9554_handle_t handle, uint8_t mask, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    const uint8_t new_value = (handle->output_shadow & (uint8_t)~mask) | (value & mask);
    return tca9554_write_port(handle, new_value);
}

esp_err_t tca9554_read_pin(tca9554_handle_t handle, uint8_t pin, bool *level)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(pin <= 7, ESP_ERR_INVALID_ARG, TAG, "invalid pin");
    ESP_RETURN_ON_FALSE(level != NULL, ESP_ERR_INVALID_ARG, TAG, "level is null");

    uint8_t value = 0;
    ESP_RETURN_ON_ERROR(tca9554_read_port(handle, &value), TAG, "read port failed");
    *level = (value & (uint8_t)(1u << pin)) != 0;
    return ESP_OK;
}

esp_err_t tca9554_write_pin(tca9554_handle_t handle, uint8_t pin, bool level)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(pin <= 7, ESP_ERR_INVALID_ARG, TAG, "invalid pin");

    const uint8_t mask = (uint8_t)(1u << pin);
    return tca9554_write_port_masked(handle, mask, level ? mask : 0);
}

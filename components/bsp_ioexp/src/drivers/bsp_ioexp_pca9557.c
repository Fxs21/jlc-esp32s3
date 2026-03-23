#include "bsp_ioexp_pca9557.h"

#include <stdlib.h>

#include "esp_check.h"

#define BSP_IOEXP_PCA9557_TAG "bsp_ioexp_pca9557"

#define PCA9557_OUTPUT_PORT_REG       0x01
#define PCA9557_POLARITY_INV_PORT_REG 0x02
#define PCA9557_CONFIG_PORT_REG       0x03
#define PCA9557_TIMEOUT_MS            1000

struct bsp_ioexp_pca9557_s {
    i2c_master_dev_handle_t dev;
    uint8_t output_shadow;
};

static esp_err_t pca9557_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = { reg, value };
    return i2c_master_transmit(dev, payload, sizeof(payload), PCA9557_TIMEOUT_MS);
}

esp_err_t bsp_ioexp_pca9557_open(const bsp_ioexp_pca9557_cfg_t *cfg, bsp_ioexp_pca9557_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, BSP_IOEXP_PCA9557_TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->dev != NULL, ESP_ERR_INVALID_ARG, BSP_IOEXP_PCA9557_TAG, "dev is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_IOEXP_PCA9557_TAG, "out_handle is null");

    struct bsp_ioexp_pca9557_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_IOEXP_PCA9557_TAG, "no memory");

    esp_err_t ret = pca9557_write_reg(cfg->dev, PCA9557_OUTPUT_PORT_REG, cfg->output_default);
    if (ret == ESP_OK) {
        ret = pca9557_write_reg(cfg->dev, PCA9557_CONFIG_PORT_REG, cfg->direction_mask);
    }
    if (ret == ESP_OK) {
        ret = pca9557_write_reg(cfg->dev, PCA9557_POLARITY_INV_PORT_REG, 0x00);
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

esp_err_t bsp_ioexp_pca9557_close(bsp_ioexp_pca9557_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_IOEXP_PCA9557_TAG, "handle is null");
    free(handle);
    return ESP_OK;
}

esp_err_t bsp_ioexp_pca9557_set_level(bsp_ioexp_pca9557_handle_t handle, uint8_t pin, bool active_high, bool asserted)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_IOEXP_PCA9557_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(pin <= 7, ESP_ERR_INVALID_ARG, BSP_IOEXP_PCA9557_TAG, "invalid pin");

    const uint8_t old_shadow = handle->output_shadow;
    const bool level = asserted ? active_high : !active_high;
    const uint8_t mask = (uint8_t)(1u << pin);
    if (level) {
        handle->output_shadow |= mask;
    } else {
        handle->output_shadow &= (uint8_t)(~mask);
    }

    esp_err_t ret = pca9557_write_reg(handle->dev, PCA9557_OUTPUT_PORT_REG, handle->output_shadow);
    if (ret != ESP_OK) {
        handle->output_shadow = old_shadow;
    }
    return ret;
}

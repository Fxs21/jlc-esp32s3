#include "doers3_ioexp.h"

#include <string.h>

#include "doers3_i2c.h"
#include "doers3_pins.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "pca9557.h"

#define TAG "doers3_ioexp"

static i2c_master_dev_handle_t s_dev;
static pca9557_handle_t s_chip;
static bool s_bus_acquired;

static esp_err_t driver_init(void *ctx)
{
    (void)ctx;

    esp_err_t ret = doers3_i2c_acquire();
    if (ret != ESP_OK) {
        return ret;
    }
    s_bus_acquired = true;

    i2c_master_bus_handle_t bus = NULL;
    ret = doers3_i2c_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DOERS3_IOEXP_I2C_ADDR,
        .scl_speed_hz = DOERS3_IOEXP_SPEED_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        goto err;
    }

    pca9557_config_t pca_cfg = {
        .dev = s_dev,
        .output_default = DOERS3_IOEXP_OUTPUT_DEFAULT,
        .direction_mask = DOERS3_IOEXP_DIRECTION_MASK,
    };
    ret = pca9557_open(&pca_cfg, &s_chip);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    (void)i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;

err:
    if (s_bus_acquired) {
        (void)doers3_i2c_release();
        s_bus_acquired = false;
    }
    return ret;
}

static void driver_deinit(void *ctx)
{
    (void)ctx;

    if (s_chip != NULL) {
        (void)pca9557_close(s_chip);
        s_chip = NULL;
    }
    if (s_dev != NULL) {
        (void)i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (s_bus_acquired) {
        (void)doers3_i2c_release();
        s_bus_acquired = false;
    }
}

static esp_err_t driver_write_pin(void *ctx, uint8_t pin, bool level)
{
    (void)ctx;
    return pca9557_write_pin(s_chip, pin, level);
}

static const bsp_ioexp_driver_t s_driver = {
    .init = driver_init,
    .deinit = driver_deinit,
    .write_pin = driver_write_pin,
    .read_pin = NULL,
    .ctx = NULL,
};

esp_err_t doers3_ioexp_acquire(void)
{
    return bsp_ioexp_acquire(&s_driver);
}

esp_err_t doers3_ioexp_release(void)
{
    return bsp_ioexp_release();
}

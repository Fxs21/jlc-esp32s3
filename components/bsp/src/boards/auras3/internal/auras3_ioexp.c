#include "auras3_ioexp.h"

#include <string.h>

#include "auras3_i2c.h"
#include "auras3_pins.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "tca9554.h"

#define TAG "auras3_ioexp"

static i2c_master_dev_handle_t s_dev;
static tca9554_handle_t s_chip;
static bool s_bus_acquired;

static esp_err_t driver_init(void *ctx)
{
    (void)ctx;

    esp_err_t ret = auras3_i2c_acquire();
    if (ret != ESP_OK) {
        return ret;
    }
    s_bus_acquired = true;

    i2c_master_bus_handle_t bus = NULL;
    ret = auras3_i2c_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AURAS3_TCA9554_I2C_ADDR,
        .scl_speed_hz = AURAS3_TCA9554_SPEED_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        goto err;
    }

    #define TCA9554_OUTPUT_DEFAULT 0xFF
    #define TCA9554_DIRECTION_MASK 0x7F
    tca9554_config_t chip_cfg = {
        .dev = s_dev,
        .output_default = TCA9554_OUTPUT_DEFAULT,
        .direction_mask = TCA9554_DIRECTION_MASK,
    };
    ret = tca9554_open(&chip_cfg, &s_chip);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    (void)i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;

err:
    if (s_bus_acquired) {
        (void)auras3_i2c_release();
        s_bus_acquired = false;
    }
    return ret;
}

static void driver_deinit(void *ctx)
{
    (void)ctx;

    if (s_chip != NULL) {
        (void)tca9554_close(s_chip);
        s_chip = NULL;
    }
    if (s_dev != NULL) {
        (void)i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (s_bus_acquired) {
        (void)auras3_i2c_release();
        s_bus_acquired = false;
    }
}

static esp_err_t driver_write_pin(void *ctx, uint8_t pin, bool level)
{
    (void)ctx;
    return tca9554_write_pin(s_chip, pin, level);
}

static esp_err_t driver_read_pin(void *ctx, uint8_t pin, bool *out_level)
{
    (void)ctx;
    return tca9554_read_pin(s_chip, pin, out_level);
}

static const bsp_ioexp_driver_t s_driver = {
    .init = driver_init,
    .deinit = driver_deinit,
    .write_pin = driver_write_pin,
    .read_pin = driver_read_pin,
    .ctx = NULL,
};

esp_err_t auras3_ioexp_acquire(void)
{
    return bsp_ioexp_acquire(&s_driver);
}

esp_err_t auras3_ioexp_release(void)
{
    return bsp_ioexp_release();
}

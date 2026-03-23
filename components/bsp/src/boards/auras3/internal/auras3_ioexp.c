#include "auras3_ioexp.h"

#include <string.h>

#include "bsp_i2c.h"
#include "auras3_pins.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tca9554.h"

#define TAG "auras3_ioexp"
#define LOCK_TIMEOUT_MS 1000

static i2c_master_dev_handle_t s_dev;
static tca9554_handle_t s_chip;
static uint32_t s_ref_count;
static bool s_bus_acquired;

static StaticSemaphore_t s_lock_buf;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t lock_take(void)
{
    if (s_lock == NULL) {
        taskENTER_CRITICAL(&s_lock_mux);
        if (s_lock == NULL) {
            s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf);
        }
        taskEXIT_CRITICAL(&s_lock_mux);
        if (s_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return xSemaphoreTake(s_lock, pdMS_TO_TICKS(LOCK_TIMEOUT_MS)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void lock_give(void)
{
    (void)xSemaphoreGive(s_lock);
}

esp_err_t auras3_ioexp_acquire(void)
{
    esp_err_t ret = lock_take();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ref_count > 0) {
        s_ref_count++;
        ret = ESP_OK;
        goto out;
    }

    i2c_master_bus_handle_t bus = NULL;
    ret = bsp_i2c_acquire(&bus);
    if (ret != ESP_OK) {
        goto out;
    }
    s_bus_acquired = true;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AURAS3_TCA9554_I2C_ADDR,
        .scl_speed_hz = AURAS3_TCA9554_SPEED_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        goto err;
    }

    tca9554_config_t chip_cfg = {
        .dev = s_dev,
        .output_default = 0xFF,
        .direction_mask = 0x7F,
    };
    ret = tca9554_open(&chip_cfg, &s_chip);
    if (ret == ESP_OK) {
        s_ref_count = 1;
        ret = ESP_OK;
        goto out;
    }

    (void)i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;

err:
    if (s_bus_acquired) {
        (void)bsp_i2c_release();
        s_bus_acquired = false;
    }

out:
    lock_give();
    return ret;
}

esp_err_t auras3_ioexp_release(void)
{
    esp_err_t ret = lock_take();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    if (s_ref_count > 1) {
        s_ref_count--;
        ret = ESP_OK;
        goto out;
    }

    if (s_chip != NULL) {
        (void)tca9554_close(s_chip);
        s_chip = NULL;
    }
    if (s_dev != NULL) {
        (void)i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (s_bus_acquired) {
        (void)bsp_i2c_release();
        s_bus_acquired = false;
    }
    s_ref_count = 0;
    ret = ESP_OK;

out:
    lock_give();
    return ret;
}

esp_err_t auras3_ioexp_set_pin(uint8_t pin, bool level)
{
    if (s_chip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return tca9554_write_pin(s_chip, pin, level);
}

esp_err_t auras3_ioexp_get_pin(uint8_t pin, bool *out_level)
{
    ESP_RETURN_ON_FALSE(out_level != NULL, ESP_ERR_INVALID_ARG, TAG, "out_level is null");
    if (s_chip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return tca9554_read_pin(s_chip, pin, out_level);
}

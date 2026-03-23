#include "doers3_ioexp.h"

#include <string.h>

#include "doers3_i2c.h"
#include "doers3_pins.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "pca9557.h"

#define TAG "doers3_ioexp"
#define LOCK_TIMEOUT_MS 1000

typedef struct {
    i2c_master_dev_handle_t dev;
    pca9557_handle_t driver;
    uint32_t ref_count;
    bool bus_acquired;
} doers3_ioexp_state_t;

static doers3_ioexp_state_t s_ioexp;
static StaticSemaphore_t s_lock_buf;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t ensure_lock(void)
{
    if (s_lock != NULL) {
        return ESP_OK;
    }
    taskENTER_CRITICAL(&s_lock_mux);
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf);
    }
    taskEXIT_CRITICAL(&s_lock_mux);
    ESP_RETURN_ON_FALSE(s_lock != NULL, ESP_ERR_NO_MEM, TAG, "create lock failed");
    return ESP_OK;
}

static esp_err_t lock(void)
{
    ESP_RETURN_ON_ERROR(ensure_lock(), TAG, "lock not ready");
    return xSemaphoreTake(s_lock, pdMS_TO_TICKS(LOCK_TIMEOUT_MS)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void unlock(void)
{
    if (s_lock != NULL) {
        (void)xSemaphoreGive(s_lock);
    }
}

esp_err_t doers3_ioexp_acquire(void)
{
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ioexp.ref_count > 0) {
        s_ioexp.ref_count++;
        ret = ESP_OK;
        goto out;
    }

    ret = doers3_i2c_acquire();
    if (ret != ESP_OK) {
        goto out;
    }
    s_ioexp.bus_acquired = true;

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
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_ioexp.dev);
    if (ret != ESP_OK) {
        goto err;
    }

    pca9557_config_t pca_cfg = {
        .dev = s_ioexp.dev,
        .output_default = DOERS3_IOEXP_OUTPUT_DEFAULT,
        .direction_mask = DOERS3_IOEXP_DIRECTION_MASK,
    };
    ret = pca9557_open(&pca_cfg, &s_ioexp.driver);
    if (ret != ESP_OK) {
        goto err;
    }

    s_ioexp.ref_count = 1;
    ret = ESP_OK;
    goto out;

err:
    if (s_ioexp.driver != NULL) {
        (void)pca9557_close(s_ioexp.driver);
    }
    if (s_ioexp.dev != NULL) {
        (void)i2c_master_bus_rm_device(s_ioexp.dev);
    }
    if (s_ioexp.bus_acquired) {
        (void)doers3_i2c_release();
    }
    memset(&s_ioexp, 0, sizeof(s_ioexp));

out:
    unlock();
    return ret;
}

esp_err_t doers3_ioexp_release(void)
{
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ioexp.ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }
    if (s_ioexp.ref_count > 1) {
        s_ioexp.ref_count--;
        ret = ESP_OK;
        goto out;
    }

    esp_err_t first_err = ESP_OK;
    if (s_ioexp.driver != NULL) {
        first_err = pca9557_close(s_ioexp.driver);
    }
    if (s_ioexp.dev != NULL) {
        esp_err_t e = i2c_master_bus_rm_device(s_ioexp.dev);
        if (e != ESP_OK && first_err == ESP_OK) {
            first_err = e;
        }
    }
    if (s_ioexp.bus_acquired) {
        esp_err_t e = doers3_i2c_release();
        if (e != ESP_OK && first_err == ESP_OK) {
            first_err = e;
        }
    }

    if (first_err == ESP_OK) {
        memset(&s_ioexp, 0, sizeof(s_ioexp));
    }
    ret = first_err;

out:
    unlock();
    return ret;
}

esp_err_t doers3_ioexp_write_pin(uint8_t pin, bool level)
{
    ESP_RETURN_ON_FALSE(pin <= 7, ESP_ERR_INVALID_ARG, TAG, "invalid pin");
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }
    if (s_ioexp.ref_count == 0 || s_ioexp.driver == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ret = pca9557_write_pin(s_ioexp.driver, pin, level);
    }
    unlock();
    return ret;
}

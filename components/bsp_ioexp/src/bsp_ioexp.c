#include "bsp_ioexp.h"

#include <string.h>

#include "bsp_boarddb.h"
#include "bsp_i2c_bus.h"
#include "bsp_ioexp_pca9557.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define BSP_IOEXP_TAG "bsp_ioexp"

#define BSP_IOEXP_LOCK_TIMEOUT_MS     1000

typedef struct {
    i2c_master_dev_handle_t dev;
    bsp_ioexp_pca9557_handle_t driver;
    uint32_t ref_count;
    bool bus_opened;
} bsp_ioexp_state_t;

static bsp_ioexp_state_t ioexp_state;
static StaticSemaphore_t ioexp_lock_buf;
static SemaphoreHandle_t ioexp_lock_handle;
static portMUX_TYPE ioexp_lock_init_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t ioexp_ensure_lock(void)
{
    if (ioexp_lock_handle != NULL) {
        return ESP_OK;
    }

    taskENTER_CRITICAL(&ioexp_lock_init_mux);
    if (ioexp_lock_handle == NULL) {
        ioexp_lock_handle = xSemaphoreCreateMutexStatic(&ioexp_lock_buf);
    }
    taskEXIT_CRITICAL(&ioexp_lock_init_mux);

    ESP_RETURN_ON_FALSE(ioexp_lock_handle != NULL, ESP_ERR_NO_MEM, BSP_IOEXP_TAG, "create lock failed");
    return ESP_OK;
}

static esp_err_t ioexp_lock(void)
{
    ESP_RETURN_ON_ERROR(ioexp_ensure_lock(), BSP_IOEXP_TAG, "lock not ready");
    if (xSemaphoreTake(ioexp_lock_handle, pdMS_TO_TICKS(BSP_IOEXP_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void ioexp_unlock(void)
{
    if (ioexp_lock_handle != NULL) {
        (void)xSemaphoreGive(ioexp_lock_handle);
    }
}

static const bsp_boarddb_ioexp_desc_t *ioexp_cfg(void)
{
    return &bsp_boarddb_get()->ioexp;
}

esp_err_t bsp_ioexp_open(void)
{
    esp_err_t ret = ioexp_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (ioexp_state.ref_count > 0) {
        ioexp_state.ref_count++;
        ret = ESP_OK;
        goto out;
    }

    ESP_GOTO_ON_FALSE(ioexp_cfg()->present, ESP_ERR_NOT_SUPPORTED, out, BSP_IOEXP_TAG, "ioexp not present");
    ESP_GOTO_ON_FALSE(ioexp_cfg()->kind == BSP_BOARDDB_IOEXP_KIND_PCA9557,
                      ESP_ERR_NOT_SUPPORTED, out, BSP_IOEXP_TAG, "ioexp kind not supported");

    bsp_i2c_bus_cfg_t bus_cfg = bsp_i2c_bus_default_cfg();
    ret = bsp_i2c_bus_open(&bus_cfg);
    if (ret != ESP_OK) {
        goto out;
    }
    ioexp_state.bus_opened = true;

    i2c_master_bus_handle_t bus = NULL;
    ret = bsp_i2c_bus_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ioexp_cfg()->address,
        .scl_speed_hz = ioexp_cfg()->scl_speed_hz,
    };

    i2c_master_dev_handle_t dev = NULL;
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (ret != ESP_OK) {
        goto err;
    }

    bsp_ioexp_pca9557_cfg_t driver_cfg = {
        .dev = dev,
        .output_default = ioexp_cfg()->output_default,
        .direction_mask = ioexp_cfg()->direction_mask,
    };
    ret = bsp_ioexp_pca9557_open(&driver_cfg, &ioexp_state.driver);
    if (ret != ESP_OK) {
        (void)i2c_master_bus_rm_device(dev);
        goto err;
    }

    ioexp_state.dev = dev;
    ioexp_state.ref_count = 1;
    ret = ESP_OK;
    goto out;

err:
    if (ioexp_state.bus_opened) {
        (void)bsp_i2c_bus_close();
        ioexp_state.bus_opened = false;
    }

out:
    ioexp_unlock();
    return ret;
}

esp_err_t bsp_ioexp_close(void)
{
    esp_err_t ret = ioexp_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (ioexp_state.ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    if (ioexp_state.ref_count > 1) {
        ioexp_state.ref_count--;
        ret = ESP_OK;
        goto out;
    }

    esp_err_t first_err = ESP_OK;
    if (ioexp_state.driver != NULL) {
        first_err = bsp_ioexp_pca9557_close(ioexp_state.driver);
        if (first_err == ESP_OK) {
            ioexp_state.driver = NULL;
        }
    }

    if (ioexp_state.dev != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(ioexp_state.dev);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
        if (ret == ESP_OK) {
            ioexp_state.dev = NULL;
        }
    }

    if (first_err == ESP_OK && ioexp_state.bus_opened) {
        first_err = bsp_i2c_bus_close();
    }

    if (first_err == ESP_OK) {
        memset(&ioexp_state, 0, sizeof(ioexp_state));
    } else if (ioexp_state.ref_count == 1) {
        ioexp_state.ref_count = 1;
    }

    ret = first_err;

out:
    ioexp_unlock();
    return ret;
}

esp_err_t bsp_ioexp_set_level(bsp_ioexp_pin_t pin, bool asserted)
{
    ESP_RETURN_ON_FALSE(pin.pin <= 7, ESP_ERR_INVALID_ARG, BSP_IOEXP_TAG, "invalid pin");

    esp_err_t ret = ioexp_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (ioexp_state.ref_count == 0 || ioexp_state.driver == NULL) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    ret = bsp_ioexp_pca9557_set_level(ioexp_state.driver, pin.pin, pin.active_high, asserted);

out:
    ioexp_unlock();
    return ret;
}

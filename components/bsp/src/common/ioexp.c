#include "ioexp.h"

#include <string.h>

#include "lock.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "bsp_ioexp"
#define LOCK_TIMEOUT_MS 1000

typedef struct {
    const bsp_ioexp_driver_t *driver;
    uint32_t ref_count;
    bool driver_inited;
} bsp_ioexp_state_t;

static bsp_ioexp_state_t s_ioexp;
static StaticSemaphore_t s_lock_buf;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t lock(void)
{
    return bsp_lock_take(&s_lock, &s_lock_buf, &s_lock_mux, TAG, LOCK_TIMEOUT_MS);
}

static void unlock(void)
{
    bsp_lock_give(s_lock);
}

esp_err_t bsp_ioexp_acquire(const bsp_ioexp_driver_t *driver)
{
    ESP_RETURN_ON_FALSE(driver != NULL, ESP_ERR_INVALID_ARG, TAG, "driver is null");

    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ioexp.ref_count > 0) {
        s_ioexp.ref_count++;
        ret = ESP_OK;
        goto out;
    }

    s_ioexp.driver = driver;
    if (driver->init != NULL) {
        ret = driver->init(driver->ctx);
        if (ret != ESP_OK) {
            s_ioexp.driver = NULL;
            goto out;
        }
    }
    s_ioexp.driver_inited = true;
    s_ioexp.ref_count = 1;
    ret = ESP_OK;

out:
    unlock();
    return ret;
}

esp_err_t bsp_ioexp_release(void)
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

    if (s_ioexp.driver != NULL && s_ioexp.driver->deinit != NULL) {
        s_ioexp.driver->deinit(s_ioexp.driver->ctx);
    }
    memset(&s_ioexp, 0, sizeof(s_ioexp));
    ret = ESP_OK;

out:
    unlock();
    return ret;
}

esp_err_t bsp_ioexp_set_pin(uint8_t pin, bool level)
{
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ioexp.ref_count == 0 || s_ioexp.driver == NULL || s_ioexp.driver->write_pin == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ret = s_ioexp.driver->write_pin(s_ioexp.driver->ctx, pin, level);
    }

    unlock();
    return ret;
}

esp_err_t bsp_ioexp_get_pin(uint8_t pin, bool *out_level)
{
    ESP_RETURN_ON_FALSE(out_level != NULL, ESP_ERR_INVALID_ARG, TAG, "out_level is null");

    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ioexp.ref_count == 0 || s_ioexp.driver == NULL || s_ioexp.driver->read_pin == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ret = s_ioexp.driver->read_pin(s_ioexp.driver->ctx, pin, out_level);
    }

    unlock();
    return ret;
}

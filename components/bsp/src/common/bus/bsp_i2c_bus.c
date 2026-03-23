#include "bsp_i2c_bus.h"

#include <stdlib.h>
#include <string.h>

#include "bsp_i2c.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lock.h"

#define TAG "bsp_i2c_bus"
#define LOCK_TIMEOUT_MS 1000

struct bsp_i2c_bus {
    bsp_i2c_bus_config_t config;
    i2c_master_bus_handle_t bus;
    uint32_t ref_count;
};

static StaticSemaphore_t s_lock_buf;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;

static const char *cfg_tag(const bsp_i2c_bus_config_t *config)
{
    return (config != NULL && config->tag != NULL) ? config->tag : TAG;
}

static esp_err_t lock(void)
{
    return bsp_lock_take(&s_lock, &s_lock_buf, &s_lock_mux, TAG, LOCK_TIMEOUT_MS);
}

static void unlock(void)
{
    bsp_lock_give(s_lock);
}

static esp_err_t prepare_lines(const bsp_i2c_bus_config_t *config)
{
    const char *log_tag = cfg_tag(config);
    const int half_period_us = 5;
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << config->sda) | (1ULL << config->scl),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = config->internal_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), log_tag, "configure recovery gpio failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(config->scl, 0), log_tag, "drive scl low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(config->sda, 1), log_tag, "release sda failed");
    esp_rom_delay_us(half_period_us);

    int i = 0;
    while (!gpio_get_level(config->sda) && (i++ < 9)) {
        ESP_RETURN_ON_ERROR(gpio_set_level(config->scl, 1), log_tag, "release scl pulse failed");
        esp_rom_delay_us(half_period_us);
        ESP_RETURN_ON_ERROR(gpio_set_level(config->scl, 0), log_tag, "drive scl pulse low failed");
        esp_rom_delay_us(half_period_us);
    }

    ESP_RETURN_ON_ERROR(gpio_set_level(config->sda, 0), log_tag, "drive sda low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(config->scl, 1), log_tag, "release scl failed");
    esp_rom_delay_us(half_period_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(config->sda, 1), log_tag, "release sda failed");

    if (gpio_get_level(config->sda) == 0 || gpio_get_level(config->scl) == 0) {
        ESP_LOGW(log_tag, "manual clear failed");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t bsp_i2c_bus_acquire(bsp_i2c_bus_handle_t *handle, const bsp_i2c_bus_config_t *config)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");

    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    bsp_i2c_bus_handle_t bus = *handle;
    if (bus != NULL && bus->ref_count > 0) {
        bus->ref_count++;
        ret = ESP_OK;
        goto out;
    }

    bus = calloc(1, sizeof(*bus));
    if (bus == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto out;
    }
    bus->config = *config;

    ret = prepare_lines(config);
    if (ret != ESP_OK) {
        goto err;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = config->port,
        .sda_io_num = config->sda,
        .scl_io_num = config->scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = config->glitch_ignore_cnt,
        .flags.enable_internal_pullup = config->internal_pullup,
    };
    ret = i2c_new_master_bus(&bus_cfg, &bus->bus);
    if (ret == ESP_OK) {
        ret = i2c_master_bus_reset(bus->bus);
    }
    if (ret != ESP_OK) {
        if (bus->bus != NULL) {
            (void)i2c_del_master_bus(bus->bus);
        }
        goto err;
    }

    bus->ref_count = 1;
    *handle = bus;
    ret = ESP_OK;
    goto out;

err:
    free(bus);
    *handle = NULL;

out:
    unlock();
    return ret;
}

esp_err_t bsp_i2c_bus_release(bsp_i2c_bus_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");

    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    bsp_i2c_bus_handle_t bus = *handle;
    if (bus == NULL || bus->ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    bus->ref_count--;
    if (bus->ref_count == 0) {
        ret = i2c_del_master_bus(bus->bus);
        if (ret == ESP_OK) {
            memset(bus, 0, sizeof(*bus));
            free(bus);
            *handle = NULL;
        } else {
            bus->ref_count = 1;
        }
    } else {
        ret = ESP_OK;
    }

out:
    unlock();
    return ret;
}

esp_err_t bsp_i2c_bus_get(bsp_i2c_bus_handle_t handle, i2c_master_bus_handle_t *out_bus)
{
    ESP_RETURN_ON_FALSE(out_bus != NULL, ESP_ERR_INVALID_ARG, TAG, "out_bus is null");
    *out_bus = NULL;

    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }
    if (handle == NULL || handle->ref_count == 0 || handle->bus == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        *out_bus = handle->bus;
        ret = ESP_OK;
    }
    unlock();
    return ret;
}

esp_err_t bsp_i2c_bus_probe(bsp_i2c_bus_handle_t handle, uint8_t address, uint32_t timeout_ms)
{
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(bsp_i2c_bus_get(handle, &bus), TAG, "get bus failed");

    uint32_t probe_timeout_ms = timeout_ms == 0 ? BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS : timeout_ms;
    return i2c_master_probe(bus, address, probe_timeout_ms);
}

esp_err_t bsp_i2c_bus_scan(bsp_i2c_bus_handle_t handle, uint8_t *out_addresses, size_t address_capacity,
                           size_t *out_count, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(address_capacity == 0 || out_addresses != NULL, ESP_ERR_INVALID_ARG, TAG, "addresses is null");
    ESP_RETURN_ON_FALSE(out_count != NULL, ESP_ERR_INVALID_ARG, TAG, "out_count is null");

    *out_count = 0;
    size_t found = 0;
    for (uint8_t addr = BSP_I2C_SCAN_FIRST_ADDR; addr <= BSP_I2C_SCAN_LAST_ADDR; addr++) {
        if (bsp_i2c_bus_probe(handle, addr, timeout_ms) == ESP_OK) {
            if (found < address_capacity) {
                out_addresses[found] = addr;
            }
            found++;
        }
    }

    *out_count = found;
    return ESP_OK;
}

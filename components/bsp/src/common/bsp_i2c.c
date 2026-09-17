#include "bsp_i2c.h"
#include "bsp_i2c_internal.h"

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "bsp_i2c"

// ---------------------------------------------------------------------------
// Singleton bus with ref-count
// ---------------------------------------------------------------------------

static i2c_master_bus_handle_t s_bus;
static uint32_t s_ref_count;
static StaticSemaphore_t s_lock_buf;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_prepared;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

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
    return xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void lock_give(void)
{
    (void)xSemaphoreGive(s_lock);
}

static esp_err_t prepare_lines(const bsp_i2c_bus_config_t *cfg)
{
    const int half_period_us = 5;
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << cfg->sda) | (1ULL << cfg->scl),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = cfg->internal_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), TAG, "config recovery gpio failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 0), TAG, "drive scl low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->sda, 1), TAG, "release sda failed");
    esp_rom_delay_us(half_period_us);

    int i = 0;
    while (!gpio_get_level(cfg->sda) && (i++ < 9)) {
        ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 1), TAG, "release scl pulse failed");
        esp_rom_delay_us(half_period_us);
        ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 0), TAG, "drive scl pulse low failed");
        esp_rom_delay_us(half_period_us);
    }

    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->sda, 0), TAG, "drive sda low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 1), TAG, "release scl failed");
    esp_rom_delay_us(half_period_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->sda, 1), TAG, "release sda failed");
    esp_rom_delay_us(half_period_us);

    if (gpio_get_level(cfg->sda) == 0 || gpio_get_level(cfg->scl) == 0) {
        ESP_LOGW(TAG, "manual clear failed");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

esp_err_t bsp_i2c_acquire(i2c_master_bus_handle_t *bus_out)
{
    ESP_RETURN_ON_FALSE(bus_out != NULL, ESP_ERR_INVALID_ARG, TAG, "bus_out is null");
    *bus_out = NULL;

    esp_err_t ret = lock_take();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ref_count > 0) {
        s_ref_count++;
        *bus_out = s_bus;
        ret = ESP_OK;
        goto out;
    }

    const bsp_i2c_bus_config_t *cfg = bsp_i2c_get_config();
    if (cfg == NULL) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    if (!s_prepared) {
        ret = prepare_lines(cfg);
        if (ret != ESP_OK) {
            goto out;
        }
        s_prepared = true;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = cfg->port,
        .sda_io_num = cfg->sda,
        .scl_io_num = cfg->scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = cfg->glitch_ignore_cnt,
        .flags.enable_internal_pullup = cfg->internal_pullup,
    };
    ret = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (ret == ESP_OK) {
        ret = i2c_master_bus_reset(s_bus);
    }
    if (ret != ESP_OK) {
        if (s_bus != NULL) {
            (void)i2c_del_master_bus(s_bus);
            s_bus = NULL;
        }
        goto out;
    }

    s_ref_count = 1;
    *bus_out = s_bus;
    ret = ESP_OK;

out:
    lock_give();
    return ret;
}

esp_err_t bsp_i2c_release(void)
{
    esp_err_t ret = lock_take();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    s_ref_count--;
    if (s_ref_count == 0) {
        ret = i2c_del_master_bus(s_bus);
        if (ret == ESP_OK) {
            s_bus = NULL;
        } else {
            s_ref_count = 1;
        }
    }

out:
    lock_give();
    return ret;
}

esp_err_t bsp_i2c_probe(uint8_t address, uint32_t timeout_ms)
{
    if (address < BSP_I2C_SCAN_FIRST_ADDR || address > BSP_I2C_SCAN_LAST_ADDR) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_handle_t bus = NULL;
    esp_err_t ret = bsp_i2c_acquire(&bus);
    if (ret != ESP_OK) {
        return ret;
    }

    uint32_t probe_timeout_ms = timeout_ms == 0 ? BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS : timeout_ms;
    ret = i2c_master_probe(bus, address, probe_timeout_ms);
    esp_err_t release_ret = bsp_i2c_release();
    return ret != ESP_OK ? ret : release_ret;
}

esp_err_t bsp_i2c_scan(uint8_t *addresses_out, size_t address_capacity,
                       size_t *count_out, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(address_capacity == 0 || addresses_out != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "addresses is null");
    ESP_RETURN_ON_FALSE(count_out != NULL, ESP_ERR_INVALID_ARG, TAG, "count_out is null");

    *count_out = 0;

    i2c_master_bus_handle_t bus = NULL;
    esp_err_t ret = bsp_i2c_acquire(&bus);
    if (ret != ESP_OK) {
        return ret;
    }

    uint32_t probe_timeout_ms = timeout_ms == 0 ? BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS : timeout_ms;
    size_t found = 0;
    for (uint8_t addr = BSP_I2C_SCAN_FIRST_ADDR; addr <= BSP_I2C_SCAN_LAST_ADDR; addr++) {
        if (i2c_master_probe(bus, addr, probe_timeout_ms) == ESP_OK) {
            if (found < address_capacity) {
                addresses_out[found] = addr;
            }
            found++;
        }
    }

    *count_out = found;
    esp_err_t release_ret = bsp_i2c_release();
    if (release_ret != ESP_OK) {
        return release_ret;
    }
    return found > address_capacity ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

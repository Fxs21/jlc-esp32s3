#include "doers3_i2c.h"

#include <string.h>

#include "doers3_pins.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "doers3_i2c"
#define LOCK_TIMEOUT_MS 1000

typedef struct {
    i2c_master_bus_handle_t bus;
    uint32_t ref_count;
} doers3_i2c_state_t;

static doers3_i2c_state_t s_i2c;
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

static esp_err_t prepare_lines(void)
{
    const int half_period_us = 5;
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << DOERS3_I2C_SDA) | (1ULL << DOERS3_I2C_SCL),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = DOERS3_I2C_INTERNAL_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), TAG, "configure recovery gpio failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(DOERS3_I2C_SCL, 0), TAG, "drive scl low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(DOERS3_I2C_SDA, 1), TAG, "release sda failed");
    esp_rom_delay_us(half_period_us);

    int i = 0;
    while (!gpio_get_level(DOERS3_I2C_SDA) && (i++ < 9)) {
        ESP_RETURN_ON_ERROR(gpio_set_level(DOERS3_I2C_SCL, 1), TAG, "release scl pulse failed");
        esp_rom_delay_us(half_period_us);
        ESP_RETURN_ON_ERROR(gpio_set_level(DOERS3_I2C_SCL, 0), TAG, "drive scl pulse low failed");
        esp_rom_delay_us(half_period_us);
    }

    ESP_RETURN_ON_ERROR(gpio_set_level(DOERS3_I2C_SDA, 0), TAG, "drive sda low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(DOERS3_I2C_SCL, 1), TAG, "release scl failed");
    esp_rom_delay_us(half_period_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(DOERS3_I2C_SDA, 1), TAG, "release sda failed");

    if (gpio_get_level(DOERS3_I2C_SDA) == 0 || gpio_get_level(DOERS3_I2C_SCL) == 0) {
        ESP_LOGW(TAG, "manual clear failed");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t doers3_i2c_acquire(void)
{
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_i2c.ref_count > 0) {
        s_i2c.ref_count++;
        ret = ESP_OK;
        goto out;
    }

    ret = prepare_lines();
    if (ret != ESP_OK) {
        goto out;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = DOERS3_I2C_PORT,
        .sda_io_num = DOERS3_I2C_SDA,
        .scl_io_num = DOERS3_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = DOERS3_I2C_GLITCH_IGNORE_CNT,
        .flags.enable_internal_pullup = DOERS3_I2C_INTERNAL_PULLUP,
    };
    ret = i2c_new_master_bus(&bus_cfg, &s_i2c.bus);
    if (ret == ESP_OK) {
        ret = i2c_master_bus_reset(s_i2c.bus);
    }
    if (ret != ESP_OK) {
        if (s_i2c.bus != NULL) {
            (void)i2c_del_master_bus(s_i2c.bus);
        }
        memset(&s_i2c, 0, sizeof(s_i2c));
        goto out;
    }

    s_i2c.ref_count = 1;

out:
    unlock();
    return ret;
}

esp_err_t doers3_i2c_release(void)
{
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_i2c.ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }
    s_i2c.ref_count--;
    if (s_i2c.ref_count == 0) {
        ret = i2c_del_master_bus(s_i2c.bus);
        if (ret == ESP_OK) {
            memset(&s_i2c, 0, sizeof(s_i2c));
        } else {
            s_i2c.ref_count = 1;
        }
    } else {
        ret = ESP_OK;
    }

out:
    unlock();
    return ret;
}

esp_err_t doers3_i2c_get(i2c_master_bus_handle_t *out_bus)
{
    ESP_RETURN_ON_FALSE(out_bus != NULL, ESP_ERR_INVALID_ARG, TAG, "out_bus is null");
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }
    if (s_i2c.ref_count == 0 || s_i2c.bus == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        *out_bus = s_i2c.bus;
        ret = ESP_OK;
    }
    unlock();
    return ret;
}

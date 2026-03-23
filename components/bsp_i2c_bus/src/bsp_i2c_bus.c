#include <string.h>

#include "bsp_boarddb.h"
#include "bsp_i2c_bus.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define BSP_I2C_BUS_TAG "bsp_i2c_bus"
#define BSP_I2C_BUS_LOCK_TIMEOUT_MS 1000

typedef struct {
    i2c_master_bus_handle_t bus;
    bsp_i2c_bus_cfg_t cfg;
    bool ready;
    uint32_t ref_count;
} bsp_i2c_bus_state_t;

static bsp_i2c_bus_state_t bus_state;
static StaticSemaphore_t bus_lock_buf;
static SemaphoreHandle_t bus_lock_handle;
static portMUX_TYPE bus_lock_init_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t bsp_i2c_bus_ensure_lock(void)
{
    if (bus_lock_handle != NULL) {
        return ESP_OK;
    }

    taskENTER_CRITICAL(&bus_lock_init_mux);
    if (bus_lock_handle == NULL) {
        bus_lock_handle = xSemaphoreCreateMutexStatic(&bus_lock_buf);
    }
    taskEXIT_CRITICAL(&bus_lock_init_mux);

    ESP_RETURN_ON_FALSE(bus_lock_handle != NULL, ESP_ERR_NO_MEM, BSP_I2C_BUS_TAG, "create lock failed");
    return ESP_OK;
}

static esp_err_t bsp_i2c_bus_lock(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_bus_ensure_lock(), BSP_I2C_BUS_TAG, "lock not ready");
    if (xSemaphoreTake(bus_lock_handle, pdMS_TO_TICKS(BSP_I2C_BUS_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void bsp_i2c_bus_unlock(void)
{
    if (bus_lock_handle != NULL) {
        (void)xSemaphoreGive(bus_lock_handle);
    }
}

static bool bsp_i2c_bus_cfg_same(const bsp_i2c_bus_cfg_t *a, const bsp_i2c_bus_cfg_t *b)
{
    return a->port == b->port &&
           a->sda == b->sda &&
           a->scl == b->scl &&
           a->glitch_ignore_cnt == b->glitch_ignore_cnt &&
           a->enable_internal_pullup == b->enable_internal_pullup;
}

static esp_err_t bsp_i2c_bus_prepare_lines(const bsp_i2c_bus_cfg_t *cfg)
{
    const int scl_half_period = 5;
    const uint64_t sda_mask = (1ULL << cfg->sda);
    const uint64_t scl_mask = (1ULL << cfg->scl);
    gpio_config_t io_cfg = {
        .pin_bit_mask = sda_mask | scl_mask,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = cfg->enable_internal_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), BSP_I2C_BUS_TAG, "configure i2c recovery gpio failed");

    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 0), BSP_I2C_BUS_TAG, "drive scl low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->sda, 1), BSP_I2C_BUS_TAG, "release sda failed");
    esp_rom_delay_us(scl_half_period);

    int i = 0;
    while (!gpio_get_level(cfg->sda) && (i++ < 9)) {
        ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 1), BSP_I2C_BUS_TAG, "release scl pulse failed");
        esp_rom_delay_us(scl_half_period);
        ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 0), BSP_I2C_BUS_TAG, "drive scl pulse low failed");
        esp_rom_delay_us(scl_half_period);
    }

    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->sda, 0), BSP_I2C_BUS_TAG, "drive sda low for stop failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->scl, 1), BSP_I2C_BUS_TAG, "release scl for stop failed");
    esp_rom_delay_us(scl_half_period);
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->sda, 1), BSP_I2C_BUS_TAG, "release sda stop failed");

    const int sda_level = gpio_get_level(cfg->sda);
    const int scl_level = gpio_get_level(cfg->scl);
    if (sda_level == 0 || scl_level == 0) {
        ESP_LOGW(BSP_I2C_BUS_TAG, "manual clear failed: sda=%d scl=%d", sda_level, scl_level);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

bsp_i2c_bus_cfg_t bsp_i2c_bus_default_cfg(void)
{
    const bsp_boarddb_i2c_desc_t *board_cfg = &bsp_boarddb_get()->i2c;
    const bsp_i2c_bus_cfg_t cfg = {
        .port = board_cfg->port,
        .sda = board_cfg->sda,
        .scl = board_cfg->scl,
        .glitch_ignore_cnt = board_cfg->glitch_ignore_cnt,
        .enable_internal_pullup = board_cfg->enable_internal_pullup,
    };
    return cfg;
}

esp_err_t bsp_i2c_bus_open(const bsp_i2c_bus_cfg_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, BSP_I2C_BUS_TAG, "cfg is null");

    esp_err_t ret = bsp_i2c_bus_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (bus_state.ref_count > 0) {
        if (!bsp_i2c_bus_cfg_same(&bus_state.cfg, cfg)) {
            ret = ESP_ERR_INVALID_STATE;
            goto out;
        }
        bus_state.ref_count++;
        ret = ESP_OK;
        goto out;
    }

    ret = bsp_i2c_bus_prepare_lines(cfg);
    if (ret != ESP_OK) {
        goto out;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = cfg->port,
        .sda_io_num = cfg->sda,
        .scl_io_num = cfg->scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = cfg->glitch_ignore_cnt,
        .flags.enable_internal_pullup = cfg->enable_internal_pullup,
    };

    i2c_master_bus_handle_t bus = NULL;
    ret = i2c_new_master_bus(&bus_cfg, &bus);
    if (ret != ESP_OK) {
        goto out;
    }

    ret = i2c_master_bus_reset(bus);
    if (ret != ESP_OK) {
        (void)i2c_del_master_bus(bus);
        goto out;
    }

    bus_state.bus = bus;
    bus_state.cfg = *cfg;
    bus_state.ready = true;
    bus_state.ref_count = 1;
    ret = ESP_OK;

out:
    bsp_i2c_bus_unlock();
    return ret;
}

esp_err_t bsp_i2c_bus_close(void)
{
    esp_err_t ret = bsp_i2c_bus_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (bus_state.ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    bus_state.ref_count--;
    if (bus_state.ref_count > 0) {
        ret = ESP_OK;
        goto out;
    }

    if (bus_state.ready && bus_state.bus != NULL) {
        ret = i2c_del_master_bus(bus_state.bus);
        if (ret != ESP_OK) {
            bus_state.ref_count = 1;
            goto out;
        }
    }

    memset(&bus_state, 0, sizeof(bus_state));
    ret = ESP_OK;

out:
    bsp_i2c_bus_unlock();
    return ret;
}

esp_err_t bsp_i2c_bus_get(i2c_master_bus_handle_t *out_bus)
{
    ESP_RETURN_ON_FALSE(out_bus != NULL, ESP_ERR_INVALID_ARG, BSP_I2C_BUS_TAG, "out_bus is null");

    esp_err_t ret = bsp_i2c_bus_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (bus_state.ref_count == 0 || !bus_state.ready || bus_state.bus == NULL) {
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    *out_bus = bus_state.bus;
    ret = ESP_OK;

out:
    bsp_i2c_bus_unlock();
    return ret;
}

bool bsp_i2c_bus_is_ready(void)
{
    if (bsp_i2c_bus_lock() != ESP_OK) {
        return false;
    }
    bool ready = bus_state.ready && bus_state.bus != NULL && bus_state.ref_count > 0;
    bsp_i2c_bus_unlock();
    return ready;
}

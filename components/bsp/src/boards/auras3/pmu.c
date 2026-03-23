#include "bsp_pmu.h"

#include <stdlib.h>
#include <string.h>

#include "auras3_i2c.h"
#include "auras3_pins.h"
#include "axp2101.h"
#include "driver/i2c_master.h"
#include "esp_check.h"

#define TAG "bsp_pmu"

struct bsp_pmu_t {
    i2c_master_dev_handle_t dev;
    axp2101_handle_t axp;
    bool i2c_acquired;
};

static const bsp_pmu_desc_t s_desc = {
    .present = true,
    .model = "AXP2101",
};

static bsp_pmu_charge_state_t map_charge_state(axp2101_charge_state_t state)
{
    switch (state) {
    case AXP2101_CHARGE_STATE_TRICKLE:
        return BSP_PMU_CHARGE_STATE_TRICKLE;
    case AXP2101_CHARGE_STATE_PRECHARGE:
        return BSP_PMU_CHARGE_STATE_PRECHARGE;
    case AXP2101_CHARGE_STATE_CONSTANT_CURRENT:
        return BSP_PMU_CHARGE_STATE_CONSTANT_CURRENT;
    case AXP2101_CHARGE_STATE_CONSTANT_VOLTAGE:
        return BSP_PMU_CHARGE_STATE_CONSTANT_VOLTAGE;
    case AXP2101_CHARGE_STATE_DONE:
        return BSP_PMU_CHARGE_STATE_DONE;
    case AXP2101_CHARGE_STATE_NOT_CHARGING:
        return BSP_PMU_CHARGE_STATE_NOT_CHARGING;
    default:
        return BSP_PMU_CHARGE_STATE_UNKNOWN;
    }
}

static bsp_pmu_event_t map_events(axp2101_event_t events)
{
    bsp_pmu_event_t mapped = BSP_PMU_EVENT_NONE;
    if (events & AXP2101_EVENT_VBUS_INSERT) {
        mapped |= BSP_PMU_EVENT_VBUS_INSERT;
    }
    if (events & AXP2101_EVENT_VBUS_REMOVE) {
        mapped |= BSP_PMU_EVENT_VBUS_REMOVE;
    }
    if (events & AXP2101_EVENT_BATTERY_INSERT) {
        mapped |= BSP_PMU_EVENT_BATTERY_INSERT;
    }
    if (events & AXP2101_EVENT_BATTERY_REMOVE) {
        mapped |= BSP_PMU_EVENT_BATTERY_REMOVE;
    }
    if (events & AXP2101_EVENT_CHARGE_START) {
        mapped |= BSP_PMU_EVENT_CHARGE_START;
    }
    if (events & AXP2101_EVENT_CHARGE_DONE) {
        mapped |= BSP_PMU_EVENT_CHARGE_DONE;
    }
    if (events & AXP2101_EVENT_POWER_KEY_SHORT) {
        mapped |= BSP_PMU_EVENT_POWER_KEY_SHORT;
    }
    if (events & AXP2101_EVENT_POWER_KEY_LONG) {
        mapped |= BSP_PMU_EVENT_POWER_KEY_LONG;
    }
    return mapped;
}

static esp_err_t close_internal(bsp_pmu_handle_t pmu)
{
    esp_err_t first_err = ESP_OK;
    if (pmu->axp != NULL) {
        esp_err_t ret = axp2101_close(pmu->axp);
        pmu->axp = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (pmu->dev != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(pmu->dev);
        pmu->dev = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (pmu->i2c_acquired) {
        esp_err_t ret = auras3_i2c_release();
        pmu->i2c_acquired = false;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    return first_err;
}

const bsp_pmu_desc_t *bsp_pmu_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_pmu_open(const bsp_pmu_config_t *config, bsp_pmu_handle_t *out_pmu)
{
    ESP_RETURN_ON_FALSE(out_pmu != NULL, ESP_ERR_INVALID_ARG, TAG, "out_pmu is null");
    *out_pmu = NULL;

    const bsp_pmu_config_t default_config = BSP_PMU_CONFIG_DEFAULT();
    const bsp_pmu_config_t *cfg = config != NULL ? config : &default_config;

    bsp_pmu_handle_t pmu = calloc(1, sizeof(*pmu));
    ESP_RETURN_ON_FALSE(pmu != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    esp_err_t ret = auras3_i2c_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    pmu->i2c_acquired = true;

    i2c_master_bus_handle_t bus = NULL;
    ret = auras3_i2c_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AURAS3_AXP2101_I2C_ADDR,
        .scl_speed_hz = AURAS3_AXP2101_SPEED_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &pmu->dev);
    if (ret != ESP_OK) {
        goto err;
    }

    const axp2101_config_t axp_cfg = {
        .dev = pmu->dev,
    };
    ret = axp2101_open(&axp_cfg, &pmu->axp);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = axp2101_disable_ts_adc(pmu->axp);
    if (ret != ESP_OK) {
        goto err;
    }
    if (cfg->enable_adc) {
        ret = axp2101_enable_adc(pmu->axp);
        if (ret != ESP_OK) {
            goto err;
        }
    }
    if (cfg->enable_irq) {
        ret = axp2101_enable_default_irqs(pmu->axp);
        if (ret != ESP_OK) {
            goto err;
        }
    }

    *out_pmu = pmu;
    return ESP_OK;

err:
    (void)close_internal(pmu);
    free(pmu);
    return ret;
}

esp_err_t bsp_pmu_close(bsp_pmu_handle_t pmu)
{
    ESP_RETURN_ON_FALSE(pmu != NULL, ESP_ERR_INVALID_ARG, TAG, "pmu is null");
    esp_err_t ret = close_internal(pmu);
    free(pmu);
    return ret;
}

esp_err_t bsp_pmu_get_status(bsp_pmu_handle_t pmu, bsp_pmu_status_t *out_status)
{
    ESP_RETURN_ON_FALSE(pmu != NULL, ESP_ERR_INVALID_ARG, TAG, "pmu is null");
    ESP_RETURN_ON_FALSE(out_status != NULL, ESP_ERR_INVALID_ARG, TAG, "out_status is null");

    axp2101_status_t axp_status = {0};
    ESP_RETURN_ON_ERROR(axp2101_get_status(pmu->axp, &axp_status), TAG, "get axp status failed");

    bsp_pmu_status_t status = {0};
    status.vbus_present = axp_status.vbus_present;
    status.vbus_good = axp_status.vbus_good;
    status.battery_present = axp_status.battery_present;
    status.charging = axp_status.charging;
    status.discharging = axp_status.discharging;
    status.standby = axp_status.standby;
    status.charge_state = map_charge_state(axp_status.charge_state);
    status.battery_percent = axp_status.battery_percent;
    status.battery_voltage_mv = axp_status.battery_voltage_mv;
    status.vbus_voltage_mv = axp_status.vbus_voltage_mv;
    status.system_voltage_mv = axp_status.system_voltage_mv;
    status.pmu_temperature_c = axp_status.temperature_c;

    *out_status = status;
    return ESP_OK;
}

esp_err_t bsp_pmu_get_events(bsp_pmu_handle_t pmu, bsp_pmu_event_t *out_events, bool clear)
{
    ESP_RETURN_ON_FALSE(pmu != NULL, ESP_ERR_INVALID_ARG, TAG, "pmu is null");
    ESP_RETURN_ON_FALSE(out_events != NULL, ESP_ERR_INVALID_ARG, TAG, "out_events is null");

    axp2101_event_t events = AXP2101_EVENT_NONE;
    ESP_RETURN_ON_ERROR(axp2101_get_events(pmu->axp, &events, clear), TAG, "get events failed");
    *out_events = map_events(events);
    return ESP_OK;
}

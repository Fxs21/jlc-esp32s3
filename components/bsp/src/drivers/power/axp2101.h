#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct axp2101_t *axp2101_handle_t;

typedef enum {
    AXP2101_CHARGE_STATE_TRICKLE = 0,
    AXP2101_CHARGE_STATE_PRECHARGE,
    AXP2101_CHARGE_STATE_CONSTANT_CURRENT,
    AXP2101_CHARGE_STATE_CONSTANT_VOLTAGE,
    AXP2101_CHARGE_STATE_DONE,
    AXP2101_CHARGE_STATE_NOT_CHARGING,
    AXP2101_CHARGE_STATE_UNKNOWN,
} axp2101_charge_state_t;

typedef enum {
    AXP2101_POWER_STATE_STANDBY = 0,
    AXP2101_POWER_STATE_CHARGING,
    AXP2101_POWER_STATE_DISCHARGING,
    AXP2101_POWER_STATE_UNKNOWN,
} axp2101_power_state_t;

typedef uint32_t axp2101_event_t;

#define AXP2101_EVENT_NONE            0u
#define AXP2101_EVENT_VBUS_INSERT     (1u << 0)
#define AXP2101_EVENT_VBUS_REMOVE     (1u << 1)
#define AXP2101_EVENT_BATTERY_INSERT  (1u << 2)
#define AXP2101_EVENT_BATTERY_REMOVE  (1u << 3)
#define AXP2101_EVENT_CHARGE_START    (1u << 4)
#define AXP2101_EVENT_CHARGE_DONE     (1u << 5)
#define AXP2101_EVENT_POWER_KEY_SHORT (1u << 6)
#define AXP2101_EVENT_POWER_KEY_LONG  (1u << 7)

typedef struct {
    i2c_master_dev_handle_t dev;
} axp2101_config_t;

typedef struct {
    bool vbus_good;
    bool battery_present;
    axp2101_power_state_t power_state;
    axp2101_charge_state_t charge_state;
    int battery_percent;
    int battery_voltage_mv;
    int vbus_voltage_mv;
    int system_voltage_mv;
    float temperature_c;
} axp2101_status_t;

esp_err_t axp2101_open(const axp2101_config_t *cfg, axp2101_handle_t *handle_out);
esp_err_t axp2101_close(axp2101_handle_t handle);
esp_err_t axp2101_enable_adc(axp2101_handle_t handle);
esp_err_t axp2101_disable_ts_adc(axp2101_handle_t handle);
esp_err_t axp2101_enable_gauge(axp2101_handle_t handle);
esp_err_t axp2101_enable_irqs(axp2101_handle_t handle);
esp_err_t axp2101_get_status(axp2101_handle_t handle, axp2101_status_t *status_out);
esp_err_t axp2101_get_events(axp2101_handle_t handle, axp2101_event_t *events_out, bool clear);
esp_err_t axp2101_power_off(axp2101_handle_t handle);

#ifdef __cplusplus
}
#endif

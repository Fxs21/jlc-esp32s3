#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_pmu_t *bsp_pmu_handle_t;

typedef enum {
    BSP_PMU_CHARGE_STATE_UNKNOWN = 0,
    BSP_PMU_CHARGE_STATE_TRICKLE,
    BSP_PMU_CHARGE_STATE_PRECHARGE,
    BSP_PMU_CHARGE_STATE_CONSTANT_CURRENT,
    BSP_PMU_CHARGE_STATE_CONSTANT_VOLTAGE,
    BSP_PMU_CHARGE_STATE_DONE,
    BSP_PMU_CHARGE_STATE_NOT_CHARGING,
} bsp_pmu_charge_state_t;

typedef uint32_t bsp_pmu_event_t;

#define BSP_PMU_EVENT_NONE            0u
#define BSP_PMU_EVENT_VBUS_INSERT     (1u << 0)
#define BSP_PMU_EVENT_VBUS_REMOVE     (1u << 1)
#define BSP_PMU_EVENT_BATTERY_INSERT  (1u << 2)
#define BSP_PMU_EVENT_BATTERY_REMOVE  (1u << 3)
#define BSP_PMU_EVENT_CHARGE_START    (1u << 4)
#define BSP_PMU_EVENT_CHARGE_DONE     (1u << 5)
#define BSP_PMU_EVENT_POWER_KEY_SHORT (1u << 6)
#define BSP_PMU_EVENT_POWER_KEY_LONG  (1u << 7)

typedef struct {
    bool enable_adc;
    bool enable_irq;
} bsp_pmu_config_t;

#define BSP_PMU_CONFIG_DEFAULT() { \
    .enable_adc = true, \
    .enable_irq = true, \
}

typedef struct {
    bool present;
    const char *model;
} bsp_pmu_desc_t;

typedef struct {
    bool vbus_present;
    bool vbus_good;

    bool battery_present;
    bool charging;
    bool discharging;
    bool standby;

    bsp_pmu_charge_state_t charge_state;

    int battery_percent;      // 0..100, -1 when invalid or no battery.
    int battery_voltage_mv;   // -1 when invalid or no battery.
    int vbus_voltage_mv;      // -1 when invalid or no VBUS.
    int system_voltage_mv;    // -1 when invalid.

    float pmu_temperature_c;
} bsp_pmu_status_t;

const bsp_pmu_desc_t *bsp_pmu_get_desc(void);

// On failure, *out_pmu is set to NULL after out_pmu is validated.
esp_err_t bsp_pmu_open(const bsp_pmu_config_t *config, bsp_pmu_handle_t *out_pmu);
esp_err_t bsp_pmu_close(bsp_pmu_handle_t pmu);
esp_err_t bsp_pmu_get_status(bsp_pmu_handle_t pmu, bsp_pmu_status_t *out_status);
esp_err_t bsp_pmu_get_events(bsp_pmu_handle_t pmu, bsp_pmu_event_t *out_events, bool clear);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_pmu_t *bsp_pmu_handle_t;

// Battery current direction (AXP2101 REG01H[6:5] on the current board).
typedef enum {
    BSP_PMU_POWER_STATE_UNKNOWN = 0,
    BSP_PMU_POWER_STATE_STANDBY,
    BSP_PMU_POWER_STATE_CHARGING,
    BSP_PMU_POWER_STATE_DISCHARGING,
} bsp_pmu_power_state_t;

typedef enum {
    BSP_PMU_CHARGE_STATE_UNKNOWN = 0,
    BSP_PMU_CHARGE_STATE_TRICKLE,
    BSP_PMU_CHARGE_STATE_PRECHARGE,
    BSP_PMU_CHARGE_STATE_CONSTANT_CURRENT,
    BSP_PMU_CHARGE_STATE_CONSTANT_VOLTAGE,
    BSP_PMU_CHARGE_STATE_DONE,
    BSP_PMU_CHARGE_STATE_NOT_CHARGING,
} bsp_pmu_charge_state_t;

// Latched events. get_events() maps the board PMU's event sources onto these
// bits; sources a board does not have are never reported.
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
    bool present;
    const char *model;
} bsp_pmu_desc_t;

// Values that are not applicable (no battery, no VBUS) are -1; when the call
// succeeds, every other field is valid.
typedef struct {
    bool vbus_good;                  // External supply is within the valid range.
    bsp_pmu_power_state_t power_state;
    bsp_pmu_charge_state_t charge_state;

    bool battery_present;
    int battery_percent;             // 0..100, -1 when no battery.
    int battery_voltage_mv;          // -1 when no battery.
    int vbus_voltage_mv;             // -1 when VBUS is not good.
    int system_voltage_mv;           // -1 when unavailable.

    float pmu_temperature_c;         // PMU die temperature.
} bsp_pmu_status_t;

const bsp_pmu_desc_t *bsp_pmu_get_desc(void);

// On failure, *pmu_out is set to NULL after pmu_out is validated.
esp_err_t bsp_pmu_open(bsp_pmu_handle_t *pmu_out);
esp_err_t bsp_pmu_close(bsp_pmu_handle_t pmu);

// Read the current status. ADC-backed fields (voltages, temperature) can read 0
// for a short time after open until the first conversion completes; poll them
// until they are plausible before use.
esp_err_t bsp_pmu_get_status(bsp_pmu_handle_t pmu, bsp_pmu_status_t *status_out);

// Read the latched events. clear=true consumes exactly the latches returned by
// this call; bits set after the snapshot stay pending for the next call.
esp_err_t bsp_pmu_get_events(bsp_pmu_handle_t pmu, bsp_pmu_event_t *events_out, bool clear);

// Cut the board power (AXP2101 REG10H[0] on the current board). On success the
// system loses power, so the function may not return; the board can only be
// powered back by a PMU power-on source (e.g. KEY2). Boards without a
// controllable PMU return ESP_ERR_NOT_SUPPORTED.
esp_err_t bsp_pmu_power_off(bsp_pmu_handle_t pmu);

#ifdef __cplusplus
}
#endif

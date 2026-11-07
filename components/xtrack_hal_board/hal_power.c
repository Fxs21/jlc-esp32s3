/**
 * @file hal_power.c
 * @brief Board HAL power -- simulated battery
 *
 * Deliberate simulation, same cycle as the PC simulator (100% -> 0% -> 100%
 * over ~100 s) but driven by esp_timer.
 *
 * TODO(real): feed real data from the bsp_pmu capability (AXP2101 on AuraS3)
 * once the PMU read path is verified.
 */
#include "hal_power.h"

#include <math.h>

#include "esp_timer.h"

/* Period of one full charge/discharge cycle (seconds) */
#define POWER_CYCLE_SEC 100

void hal_power_get_info(hal_power_info_t *out_info)
{
    double uptime_s  = (double)esp_timer_get_time() / 1000000.0;
    double cycle_pos = fmod(uptime_s, (double)POWER_CYCLE_SEC);
    float  t         = (float)(cycle_pos / (double)POWER_CYCLE_SEC); /* 0.0 -> 1.0 */

    if (t < 0.5f) {
        /* Discharging */
        out_info->percentage  = (uint8_t)((1.0f - t * 2.0f) * 100.0f + 0.5f);
        out_info->is_charging = 0;
    } else {
        /* Charging */
        out_info->percentage  = (uint8_t)((t - 0.5f) * 2.0f * 100.0f + 0.5f);
        out_info->is_charging = 1;
    }

    /* Voltage roughly proportional to percentage (3.7V LiPo scale) */
    out_info->voltage = (uint16_t)(3700 + (out_info->percentage - 50) * 3);
}

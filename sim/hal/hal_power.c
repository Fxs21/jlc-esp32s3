/**
 * @file hal_power.c
 * @brief HAL power — simulated battery (SDL simulator)
 *
 * Cycles battery from 100% -> 0% -> 100% over ~100 seconds.
 */
#include "hal_power.h"
#include <sys/time.h>
#include <time.h>

/* Period of one full charge/discharge cycle (seconds) */
#define POWER_CYCLE_SEC 100

void hal_power_get_info(hal_power_info_t *out_info)
{
    /* Use time as a pseudo-random counter for the cycle */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long cycle_pos = (tv.tv_sec % POWER_CYCLE_SEC);
    float t = (float)cycle_pos / (float)POWER_CYCLE_SEC; /* 0.0 -> 1.0 */

    /* Discharge: 100% -> 0% over first half, charge: 0% -> 100% over second half */
    if (t < 0.5f) {
        /* Discharging */
        float pct = 1.0f - (t * 2.0f);
        out_info->percentage = (uint8_t)(pct * 100.0f + 0.5f);
        out_info->is_charging = 0;
    } else {
        /* Charging */
        float pct = (t - 0.5f) * 2.0f;
        out_info->percentage = (uint8_t)(pct * 100.0f + 0.5f);
        out_info->is_charging = 1;
    }

    /* Voltage roughly proportional to percentage (3.7V LiPo scale) */
    out_info->voltage = (uint16_t)(3700 + (out_info->percentage - 50) * 3);
}

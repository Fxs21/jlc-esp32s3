/**
 * @file  hal_power.h
 * @brief HAL abstraction for power/battery module
 */
#ifndef HAL_POWER_H
#define HAL_POWER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Power/Battery information structure
 */
typedef struct {
    uint16_t voltage;    /**< Battery voltage (mV) */
    uint8_t  percentage; /**< Charge percentage (0-100) */
    uint8_t  is_charging; /**< Non-zero if charging */
} hal_power_info_t;

/**
 * @brief  Get current power/battery status
 * @param  out_info: Output structure filled with current power data
 */
void hal_power_get_info(hal_power_info_t *out_info);

#ifdef __cplusplus
}
#endif

#endif /* HAL_POWER_H */

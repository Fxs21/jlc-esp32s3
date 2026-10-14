/**
 * @file  hal_clock.h
 * @brief HAL abstraction for system clock (time of day)
 */
#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Clock information structure
 */
typedef struct {
    uint16_t year;   /**< Year (e.g. 2026) */
    uint8_t  month;  /**< Month (1-12) */
    uint8_t  day;    /**< Day (1-31) */
    uint8_t  hour;   /**< Hour (0-23) */
    uint8_t  minute; /**< Minute (0-59) */
    uint8_t  second; /**< Second (0-59) */
    uint16_t millis; /**< Milliseconds (0-999) */
} hal_clock_info_t;

/**
 * @brief  Initialize the HAL clock module
 */
void hal_clock_init(void);

/**
 * @brief  Get the current date/time
 * @param  out_info: Output structure filled with current time
 */
void hal_clock_get_info(hal_clock_info_t *out_info);

#ifdef __cplusplus
}
#endif

#endif /* HAL_CLOCK_H */

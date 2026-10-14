/**
 * @file  hal.h
 * @brief Hardware Abstraction Layer -- platform-neutral init entry
 *
 * Module interfaces are declared in hal_clock.h, hal_gps.h and hal_power.h.
 * Each platform provides its own implementation of those modules:
 *   - PC simulator: hal/hal_clock.c, hal_gps.c, hal_power.c (simulated data)
 *   - board port:   its own implementations built on the BSP
 *
 * Platform-specific display/input init is NOT here; on the PC simulator it
 * lives in hal_sdl.h, on the board it comes from the BSP.
 */
#ifndef HAL_H
#define HAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialize all HAL modules (clock, GPS, power, ...)
 * @note   Call before app_init(). Each module's init may be a no-op when
 *         the corresponding hardware is absent.
 */
void hal_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_H */

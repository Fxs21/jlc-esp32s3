/**
 * @file hal_init.c
 * @brief HAL modules — unified initialisation entry point
 *
 * hal_init() is the single place where all hardware abstraction
 * modules are initialised.  On the SDL simulator most modules
 * are no-ops; the call structure ensures nothing is forgotten
 * when porting to real hardware.
 *
 * To add a new HAL module:
 *   1. Write hal_<module>.h / hal_<module>.c
 *   2. Add #include + hal_<module>_init() call below
 */
#include "hal.h"
#include "hal_clock.h"
#include "hal_power.h"
#include "hal_gps.h"

void hal_init(void)
{
    hal_clock_init();
    /* Power uses system time, no special init needed */
    /* GPS uses system time, no special init needed */
    /* Add new HAL modules here, in dependency order */
}

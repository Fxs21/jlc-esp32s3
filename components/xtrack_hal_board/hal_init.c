/**
 * @file hal_init.c
 * @brief Board HAL entry point -- same contract as the PC simulator
 *
 * Called by app_main before app_init(). Add board HAL module inits here,
 * in dependency order.
 */
#include "hal.h"

#include "hal_clock.h"

void hal_init(void)
{
    hal_clock_init();
    /* GPS and power are timer-driven, no init needed. */
}

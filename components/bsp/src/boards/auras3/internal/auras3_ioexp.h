#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "auras3_pins.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t auras3_ioexp_acquire(void);
esp_err_t auras3_ioexp_release(void);
esp_err_t auras3_ioexp_set_pin(uint8_t pin, bool level);
esp_err_t auras3_ioexp_get_pin(uint8_t pin, bool *level_out);

// Semantic wrappers: callers use these instead of raw pin numbers.
static inline esp_err_t auras3_ioexp_get_sys_out(bool *level_out)
{
    return auras3_ioexp_get_pin(AURAS3_TCA9554_IO_SYS_OUT, level_out);
}

static inline esp_err_t auras3_ioexp_get_axp_irq(bool *level_out)
{
    return auras3_ioexp_get_pin(AURAS3_TCA9554_IO_AXP_IRQ, level_out);
}

static inline esp_err_t auras3_ioexp_set_gps_reset(bool asserted)
{
    return auras3_ioexp_set_pin(AURAS3_TCA9554_IO_GPS_RST,
                                asserted ? AURAS3_TCA9554_GPS_RST_ASSERT_LEVEL : AURAS3_TCA9554_GPS_RST_RELEASE_LEVEL);
}

#ifdef __cplusplus
}
#endif

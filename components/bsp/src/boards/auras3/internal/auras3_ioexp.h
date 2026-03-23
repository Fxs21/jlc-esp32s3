#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "auras3_pins.h"
#include "ioexp.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t auras3_ioexp_acquire(void);
esp_err_t auras3_ioexp_release(void);

// Semantic wrappers around bsp_ioexp_get_pin / bsp_ioexp_set_pin.
static inline esp_err_t auras3_ioexp_get_sys_out(bool *out_level)
{
    return bsp_ioexp_get_pin(AURAS3_TCA9554_IO_SYS_OUT, out_level);
}

static inline esp_err_t auras3_ioexp_get_axp_irq(bool *out_level)
{
    return bsp_ioexp_get_pin(AURAS3_TCA9554_IO_AXP_IRQ, out_level);
}

static inline esp_err_t auras3_ioexp_set_gps_reset(bool asserted)
{
    return bsp_ioexp_set_pin(AURAS3_TCA9554_IO_GPS_RST,
                             asserted ? AURAS3_TCA9554_GPS_RST_ASSERT_LEVEL : AURAS3_TCA9554_GPS_RST_RELEASE_LEVEL);
}

#ifdef __cplusplus
}
#endif

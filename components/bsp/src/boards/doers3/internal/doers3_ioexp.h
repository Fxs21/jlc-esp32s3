#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "doers3_pins.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t doers3_ioexp_acquire(void);
esp_err_t doers3_ioexp_release(void);
esp_err_t doers3_ioexp_set_pin(uint8_t pin, bool level);

// Semantic wrappers: callers use these instead of raw pin numbers.
static inline esp_err_t doers3_ioexp_set_lcd_cs(bool selected)
{
    return doers3_ioexp_set_pin(DOERS3_IOEXP_LCD_CS, !selected);
}

static inline esp_err_t doers3_ioexp_set_audio_pa(bool on)
{
    return doers3_ioexp_set_pin(DOERS3_IOEXP_AUDIO_PA,
                                on ? DOERS3_IOEXP_AUDIO_PA_ENABLE_LEVEL : !DOERS3_IOEXP_AUDIO_PA_ENABLE_LEVEL);
}

static inline esp_err_t doers3_ioexp_set_camera_power(bool on)
{
    return doers3_ioexp_set_pin(DOERS3_IOEXP_CAMERA_PWDN, !on);
}

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_IOEXP_PIN_NONE UINT8_MAX

typedef struct {
    uint8_t pin;
    bool active_high;
} bsp_ioexp_pin_t;

esp_err_t bsp_ioexp_open(void);
esp_err_t bsp_ioexp_close(void);
esp_err_t bsp_ioexp_set_level(bsp_ioexp_pin_t pin, bool asserted);

#ifdef __cplusplus
}
#endif

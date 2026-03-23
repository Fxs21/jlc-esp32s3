#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t doers3_ioexp_acquire(void);
esp_err_t doers3_ioexp_release(void);
esp_err_t doers3_ioexp_write_pin(uint8_t pin, bool level);

#ifdef __cplusplus
}
#endif

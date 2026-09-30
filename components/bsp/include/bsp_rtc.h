#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool present;
} bsp_rtc_desc_t;

typedef struct {
    uint16_t year;  // full year, 2000..2099
    uint8_t month;  // 1..12
    uint8_t day;    // 1..31
    uint8_t hour;   // 0..23, 24-hour mode
    uint8_t minute; // 0..59
    uint8_t second; // 0..59
} bsp_rtc_time_t;

typedef struct bsp_rtc_t *bsp_rtc_handle_t;

const bsp_rtc_desc_t *bsp_rtc_get_desc(void);

// On failure, *rtc_out is set to NULL after rtc_out is validated.
esp_err_t bsp_rtc_open(bsp_rtc_handle_t *rtc_out);
esp_err_t bsp_rtc_close(bsp_rtc_handle_t rtc);

// Read the current time. *valid_out is false when the chip reports a stopped
// oscillator (OS flag), which means the time fields are not trustworthy.
esp_err_t bsp_rtc_get_time(bsp_rtc_handle_t rtc, bsp_rtc_time_t *time_out, bool *valid_out);

// Write the time, clear the OS flag and update the weekday counter.
// Out-of-range fields return ESP_ERR_INVALID_ARG and change nothing.
esp_err_t bsp_rtc_set_time(bsp_rtc_handle_t rtc, const bsp_rtc_time_t *time);

#ifdef __cplusplus
}
#endif

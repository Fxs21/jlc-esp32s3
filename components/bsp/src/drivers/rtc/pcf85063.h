#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCF85063_I2C_ADDR 0x51

typedef struct pcf85063_t *pcf85063_handle_t;

typedef struct {
    i2c_master_dev_handle_t dev;
} pcf85063_config_t;

typedef struct {
    uint16_t year;  // 2000..2099
    uint8_t month;  // 1..12
    uint8_t day;    // 1..31
    uint8_t hour;   // 0..23
    uint8_t minute; // 0..59
    uint8_t second; // 0..59
} pcf85063_time_t;

esp_err_t pcf85063_open(const pcf85063_config_t *cfg, pcf85063_handle_t *handle_out);
esp_err_t pcf85063_close(pcf85063_handle_t handle);
esp_err_t pcf85063_read_time(pcf85063_handle_t handle, pcf85063_time_t *time_out, bool *valid_out);
esp_err_t pcf85063_write_time(pcf85063_handle_t handle, const pcf85063_time_t *time);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "bsp_touch.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cst9217_s *cst9217_handle_t;

typedef struct {
    i2c_master_bus_handle_t bus;
    uint8_t i2c_addr;
    uint32_t i2c_speed_hz;
    gpio_num_t reset_gpio;
    gpio_num_t int_gpio;
    uint16_t x_max;
    uint16_t y_max;
    bool mirror_x;
    bool mirror_y;
} cst9217_config_t;

esp_err_t cst9217_open(const cst9217_config_t *config, cst9217_handle_t *out_handle);
esp_err_t cst9217_close(cst9217_handle_t handle);
esp_err_t cst9217_read(cst9217_handle_t handle,
                       bsp_touch_point_t *points,
                       size_t points_capacity,
                       size_t *points_num);

#ifdef __cplusplus
}
#endif

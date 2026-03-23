#include "ft5x06.h"

#include <stdint.h>
#include <stdlib.h>

#include "esp_check.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_touch_ft5x06.h"

#define TAG "ft5x06"
#define TOUCH_I2C_SPEED_HZ 400000

struct ft5x06_s {
    esp_lcd_panel_io_handle_t touch_io;
    esp_lcd_touch_handle_t touch;
};

esp_err_t ft5x06_open(const ft5x06_config_t *cfg, ft5x06_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    struct ft5x06_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    esp_lcd_panel_io_i2c_config_t touch_io_cfg = {
        .scl_speed_hz = TOUCH_I2C_SPEED_HZ,
        .dev_addr = cfg->address,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .flags = {
            .disable_control_phase = 1,
        },
    };
    esp_err_t ret = esp_lcd_new_panel_io_i2c(cfg->bus, &touch_io_cfg, &handle->touch_io);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    esp_lcd_touch_config_t touch_cfg = {
        .x_max = cfg->x_max,
        .y_max = cfg->y_max,
        .rst_gpio_num = cfg->reset_gpio,
        .int_gpio_num = cfg->int_gpio,
        .levels = {
            .reset = cfg->level_reset,
            .interrupt = cfg->level_interrupt,
        },
        .flags = {
            .swap_xy = cfg->swap_xy,
            .mirror_x = cfg->mirror_x,
            .mirror_y = cfg->mirror_y,
        },
    };
    ret = esp_lcd_touch_new_i2c_ft5x06(handle->touch_io, &touch_cfg, &handle->touch);
    if (ret != ESP_OK) {
        (void)ft5x06_close(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t ft5x06_close(ft5x06_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;
    if (handle->touch != NULL) {
        esp_err_t ret = esp_lcd_touch_del(handle->touch);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (handle->touch_io != NULL) {
        esp_err_t ret = esp_lcd_panel_io_del(handle->touch_io);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    free(handle);
    return first_err;
}

esp_err_t ft5x06_read(ft5x06_handle_t handle, esp_lcd_touch_point_data_t *points, size_t points_capacity, size_t *points_num)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "points is null");
    ESP_RETURN_ON_FALSE(points_num != NULL, ESP_ERR_INVALID_ARG, TAG, "points_num is null");
    ESP_RETURN_ON_FALSE(points_capacity > 0 && points_capacity <= UINT8_MAX, ESP_ERR_INVALID_ARG, TAG, "invalid capacity");

    ESP_RETURN_ON_ERROR(esp_lcd_touch_read_data(handle->touch), TAG, "read failed");
    uint8_t n = (uint8_t)points_capacity;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_get_data(handle->touch, points, &n, n), TAG, "parse failed");
    *points_num = n;
    return ESP_OK;
}

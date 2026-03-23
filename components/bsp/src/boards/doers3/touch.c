#include "bsp_touch.h"

#include <stdlib.h>

#include "doers3_i2c.h"
#include "doers3_pins.h"
#include "esp_check.h"
#include "ft6336.h"

#define TAG "bsp_touch"

struct bsp_touch_s {
    ft6336_handle_t touch;
    bool i2c_acquired;
};

static const bsp_touch_desc_t s_desc = {
    .present = true,
    .max_points = 1,
    .x_max = DOERS3_LCD_WIDTH - 1,
    .y_max = DOERS3_LCD_HEIGHT - 1,
};

const bsp_touch_desc_t *bsp_touch_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_touch_open(bsp_touch_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    esp_err_t ret = ESP_OK;

    struct bsp_touch_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    ret = doers3_i2c_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->i2c_acquired = true;

    i2c_master_bus_handle_t bus = NULL;
    ret = doers3_i2c_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    ft6336_config_t cfg = {
        .bus = bus,
        .address = DOERS3_TOUCH_ADDR,
        .x_max = DOERS3_LCD_WIDTH - 1,
        .y_max = DOERS3_LCD_HEIGHT - 1,
        .reset_gpio = DOERS3_TOUCH_RESET,
        .int_gpio = DOERS3_TOUCH_INT,
        .level_reset = 0,
        .level_interrupt = 0,
        .swap_xy = true,
        .mirror_x = false,
        .mirror_y = true,
    };
    ret = ft6336_open(&cfg, &handle->touch);
    if (ret != ESP_OK) {
        goto err;
    }

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)bsp_touch_close(handle);
    }
    return ret;
}

esp_err_t bsp_touch_close(bsp_touch_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;
    if (handle->touch != NULL) {
        first_err = ft6336_close(handle->touch);
    }
    if (handle->i2c_acquired) {
        esp_err_t ret = doers3_i2c_release();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    free(handle);
    return first_err;
}

esp_err_t bsp_touch_read(bsp_touch_handle_t handle,
                         bsp_touch_point_t *points,
                         size_t points_capacity,
                         size_t *points_num)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "points is null");
    ESP_RETURN_ON_FALSE(points_num != NULL, ESP_ERR_INVALID_ARG, TAG, "points_num is null");
    ESP_RETURN_ON_FALSE(points_capacity > 0, ESP_ERR_INVALID_ARG, TAG, "points_capacity is zero");

    ft6336_point_t raw_points[1] = {0};
    size_t raw_points_num = 0;
    size_t raw_capacity = points_capacity > 1 ? 1 : points_capacity;
    ESP_RETURN_ON_ERROR(ft6336_read(handle->touch, raw_points, raw_capacity, &raw_points_num), TAG, "read touch failed");

    for (size_t i = 0; i < raw_points_num; ++i) {
        points[i].x = raw_points[i].x;
        points[i].y = raw_points[i].y;
        points[i].pressure = raw_points[i].pressure;
        points[i].id = raw_points[i].id;
    }
    *points_num = raw_points_num;
    return ESP_OK;
}

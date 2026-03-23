#include "bsp_touch.h"

#include <stdlib.h>

#include "auras3_i2c.h"
#include "auras3_pins.h"
#include "cst9217.h"
#include "esp_check.h"

#define TAG "bsp_touch"
#define TOUCH_MAX_POINTS 1
#define TOUCH_I2C_SPEED_HZ 400000

struct bsp_touch_s {
    cst9217_handle_t driver;
    bool i2c_acquired;
};

static const bsp_touch_desc_t s_desc = {
    .present = true,
    .max_points = TOUCH_MAX_POINTS,
    .x_max = AURAS3_TOUCH_X_MAX,
    .y_max = AURAS3_TOUCH_Y_MAX,
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

    ret = auras3_i2c_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->i2c_acquired = true;

    i2c_master_bus_handle_t bus = NULL;
    ret = auras3_i2c_get(&bus);
    if (ret != ESP_OK) {
        goto err;
    }

    const cst9217_config_t touch_cfg = {
        .bus = bus,
        .i2c_addr = AURAS3_TOUCH_ADDR,
        .i2c_speed_hz = TOUCH_I2C_SPEED_HZ,
        .reset_gpio = AURAS3_TOUCH_RESET,
        .int_gpio = AURAS3_TOUCH_INT,
        .x_max = AURAS3_TOUCH_X_MAX,
        .y_max = AURAS3_TOUCH_Y_MAX,
        .mirror_x = true,
        .mirror_y = true,
    };
    ret = cst9217_open(&touch_cfg, &handle->driver);
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

    if (handle->driver != NULL) {
        first_err = cst9217_close(handle->driver);
    }
    if (handle->i2c_acquired) {
        esp_err_t ret = auras3_i2c_release();
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
    return cst9217_read(handle->driver, points, points_capacity, points_num);
}

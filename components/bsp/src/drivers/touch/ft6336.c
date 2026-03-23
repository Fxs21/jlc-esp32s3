#include "ft6336.h"

#include <stdint.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "ft6336"
#define FT6336_I2C_SPEED_HZ 400000
#define FT6336_I2C_TIMEOUT_MS 100
#define FT6336_REG_TD_STATUS 0x02
#define FT6336_POINT_REG_BYTES 6
#define FT6336_MAX_POINTS 2

struct ft6336_s {
    i2c_master_dev_handle_t dev;
    uint16_t x_max;
    uint16_t y_max;
    gpio_num_t reset_gpio;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
};

static esp_err_t read_regs(ft6336_handle_t handle, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(handle->dev, &reg, sizeof(reg), data, len, FT6336_I2C_TIMEOUT_MS);
}

static esp_err_t reset_touch(gpio_num_t reset_gpio, uint8_t level_reset)
{
    if (reset_gpio == GPIO_NUM_NC) {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << reset_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "config reset gpio failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(reset_gpio, level_reset), TAG, "assert reset failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(gpio_set_level(reset_gpio, !level_reset), TAG, "release reset failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static uint16_t clamp_u16(uint16_t value, uint16_t max_value)
{
    return value > max_value ? max_value : value;
}

static void transform_point(ft6336_handle_t handle, uint16_t *x, uint16_t *y)
{
    if (handle->swap_xy) {
        uint16_t tmp = *x;
        *x = *y;
        *y = tmp;
    }
    if (handle->mirror_x) {
        *x = *x > handle->x_max ? 0 : handle->x_max - *x;
    }
    if (handle->mirror_y) {
        *y = *y > handle->y_max ? 0 : handle->y_max - *y;
    }
    *x = clamp_u16(*x, handle->x_max);
    *y = clamp_u16(*y, handle->y_max);
}

esp_err_t ft6336_open(const ft6336_config_t *cfg, ft6336_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    ESP_RETURN_ON_ERROR(reset_touch(cfg->reset_gpio, cfg->level_reset), TAG, "reset failed");

    struct ft6336_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    handle->x_max = cfg->x_max;
    handle->y_max = cfg->y_max;
    handle->reset_gpio = cfg->reset_gpio;
    handle->swap_xy = cfg->swap_xy;
    handle->mirror_x = cfg->mirror_x;
    handle->mirror_y = cfg->mirror_y;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->address,
        .scl_speed_hz = FT6336_I2C_SPEED_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(cfg->bus, &dev_cfg, &handle->dev);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t ft6336_close(ft6336_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    if (handle->dev != NULL) {
        (void)i2c_master_bus_rm_device(handle->dev);
    }
    free(handle);
    return ESP_OK;
}

esp_err_t ft6336_read(ft6336_handle_t handle, ft6336_point_t *points, size_t points_capacity, size_t *points_num)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "points is null");
    ESP_RETURN_ON_FALSE(points_num != NULL, ESP_ERR_INVALID_ARG, TAG, "points_num is null");
    ESP_RETURN_ON_FALSE(points_capacity > 0, ESP_ERR_INVALID_ARG, TAG, "points_capacity is zero");

    uint8_t data[1 + FT6336_MAX_POINTS * FT6336_POINT_REG_BYTES] = {0};
    ESP_RETURN_ON_ERROR(read_regs(handle, FT6336_REG_TD_STATUS, data, sizeof(data)), TAG, "read touch data failed");

    size_t reported = data[0] & 0x0F;
    if (reported > FT6336_MAX_POINTS) {
        reported = 0;
    }
    size_t count = reported < points_capacity ? reported : points_capacity;

    for (size_t i = 0; i < count; ++i) {
        const uint8_t *p = &data[1 + i * FT6336_POINT_REG_BYTES];
        uint16_t x = (uint16_t)(((p[0] & 0x0F) << 8) | p[1]);
        uint16_t y = (uint16_t)(((p[2] & 0x0F) << 8) | p[3]);
        transform_point(handle, &x, &y);

        points[i].x = x;
        points[i].y = y;
        points[i].pressure = p[4];
        points[i].id = p[2] >> 4;
    }

    *points_num = count;
    return ESP_OK;
}

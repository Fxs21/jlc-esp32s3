#include "cst9217.h"

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "cst9217"
#define CST9217_MAX_POINTS 1
#define CST9217_DATA_REG 0xD000
#define CST9217_PROJECT_ID_REG 0xD204
#define CST9217_CMD_MODE_REG 0xD101
#define CST9217_CHECKCODE_REG 0xD1FC
#define CST9217_RESOLUTION_REG 0xD1F8
#define CST9217_ACK_VALUE 0xAB
#define CST9217_STATUS_DOWN 0x06
#define CST9217_DATA_LEN (CST9217_MAX_POINTS * 5 + 5)
#define CST9217_INT_ACTIVE_LEVEL 0
#define CST9217_STALE_TIMEOUT_US 1500000

struct cst9217_s {
    cst9217_config_t config;
    i2c_master_dev_handle_t dev;
    bool isr_registered;
    volatile bool report_pending;
    portMUX_TYPE lock;
    bsp_touch_point_t last_points[CST9217_MAX_POINTS];
    size_t last_points_num;
    int64_t last_report_us;
};

static esp_err_t cst9217_hw_reset(cst9217_handle_t handle)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(handle->config.reset_gpio, 0), TAG, "reset low failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(gpio_set_level(handle->config.reset_gpio, 1), TAG, "reset high failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static esp_err_t cst9217_hw_reset_after_read_fail(cst9217_handle_t handle)
{
    ESP_LOGW(TAG, "Trigger hardware reset");
    ESP_RETURN_ON_ERROR(gpio_set_level(handle->config.reset_gpio, 0), TAG, "reset low failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(gpio_set_level(handle->config.reset_gpio, 1), TAG, "reset high failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

static esp_err_t cst9217_read_reg(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t reg_buf[2] = {reg >> 8, reg & 0xFF};

    for (int retry = 0; retry < 5; ++retry) {
        esp_err_t ret = i2c_master_transmit(dev, reg_buf, sizeof(reg_buf), -1);
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "TX failed, retry %d", retry);
            vTaskDelay(pdMS_TO_TICKS(3));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(2));
        ret = i2c_master_receive(dev, data, len, -1);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGD(TAG, "RX failed, retry %d", retry);
        vTaskDelay(pdMS_TO_TICKS(3));
    }

    return ESP_FAIL;
}

static esp_err_t cst9217_write_reg(i2c_master_dev_handle_t dev, uint16_t reg, const uint8_t *data, size_t len)
{
    uint8_t reg_buf[2] = {reg >> 8, reg & 0xFF};

    for (int retry = 0; retry < 5; ++retry) {
        esp_err_t ret = i2c_master_transmit(dev, reg_buf, sizeof(reg_buf), -1);
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "Addr TX failed, retry %d", retry);
            vTaskDelay(pdMS_TO_TICKS(3));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(2));
        ret = i2c_master_transmit(dev, data, len, -1);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGD(TAG, "Data TX failed, retry %d", retry);
        vTaskDelay(pdMS_TO_TICKS(3));
    }

    return ESP_FAIL;
}

static esp_err_t cst9217_read_config(cst9217_handle_t handle)
{
    uint8_t data[4] = {0};
    const uint8_t cmd_mode[2] = {0xD1, 0x01};
    ESP_RETURN_ON_ERROR(cst9217_write_reg(handle->dev, CST9217_CMD_MODE_REG, cmd_mode, sizeof(cmd_mode)),
                        TAG,
                        "Enter command mode failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_RETURN_ON_ERROR(cst9217_read_reg(handle->dev, CST9217_CHECKCODE_REG, data, 4),
                        TAG,
                        "Read checkcode failed");
    ESP_LOGI(TAG, "Checkcode: 0x%02X%02X%02X%02X", data[0], data[1], data[2], data[3]);

    ESP_RETURN_ON_ERROR(cst9217_read_reg(handle->dev, CST9217_RESOLUTION_REG, data, 4),
                        TAG,
                        "Read resolution failed");
    uint16_t res_x = (data[1] << 8) | data[0];
    uint16_t res_y = (data[3] << 8) | data[2];
    ESP_LOGI(TAG, "Resolution X: %d, Y: %d", res_x, res_y);

    ESP_RETURN_ON_ERROR(cst9217_read_reg(handle->dev, CST9217_PROJECT_ID_REG, data, 4),
                        TAG,
                        "Read project ID failed");
    uint16_t chip_type = (data[3] << 8) | data[2];
    uint32_t project_id = (data[1] << 8) | data[0];
    ESP_LOGI(TAG, "Chip Type: 0x%04X, ProjectID: 0x%04lX", chip_type, project_id);

    return ESP_OK;
}

static void IRAM_ATTR cst9217_isr(void *arg)
{
    cst9217_handle_t handle = (cst9217_handle_t)arg;
    if (handle == NULL) {
        return;
    }

    portENTER_CRITICAL_ISR(&handle->lock);
    handle->report_pending = true;
    portEXIT_CRITICAL_ISR(&handle->lock);
}

static void cst9217_copy_cached(cst9217_handle_t handle,
                                bsp_touch_point_t *points,
                                size_t points_capacity,
                                size_t *points_num)
{
    portENTER_CRITICAL(&handle->lock);
    size_t copy_count = handle->last_points_num;
    if (copy_count > points_capacity) {
        copy_count = points_capacity;
    }
    if (copy_count > 0) {
        memcpy(points, handle->last_points, copy_count * sizeof(points[0]));
    }
    *points_num = copy_count;
    portEXIT_CRITICAL(&handle->lock);
}

static void cst9217_update_cached(cst9217_handle_t handle,
                                  const bsp_touch_point_t *points,
                                  size_t points_num)
{
    int64_t now_us = esp_timer_get_time();

    portENTER_CRITICAL(&handle->lock);
    if (points_num > 0) {
        memcpy(handle->last_points, points, points_num * sizeof(points[0]));
    }
    handle->last_points_num = points_num;
    handle->last_report_us = now_us;
    portEXIT_CRITICAL(&handle->lock);
}

static void cst9217_clear_stale_cached(cst9217_handle_t handle)
{
    int64_t now_us = esp_timer_get_time();

    portENTER_CRITICAL(&handle->lock);
    if (handle->last_points_num > 0 &&
        handle->last_report_us > 0 &&
        now_us - handle->last_report_us > CST9217_STALE_TIMEOUT_US) {
        handle->last_points_num = 0;
    }
    portEXIT_CRITICAL(&handle->lock);
}

static size_t cst9217_parse_points(cst9217_handle_t handle,
                                   const uint8_t *data,
                                   bsp_touch_point_t *points,
                                   size_t points_capacity)
{
    if (data[6] != CST9217_ACK_VALUE) {
        return 0;
    }

    uint8_t report_points = data[5] & 0x7F;
    if (report_points > CST9217_MAX_POINTS) {
        report_points = CST9217_MAX_POINTS;
    }
    if (report_points > points_capacity) {
        report_points = points_capacity;
    }

    size_t points_num = 0;
    for (size_t i = 0; i < report_points; ++i) {
        const uint8_t *p = &data[i * 5 + (i ? 2 : 0)];
        uint8_t status = p[0] & 0x0F;
        if (status != CST9217_STATUS_DOWN) {
            continue;
        }

        uint16_t x = (p[1] << 4) | (p[3] >> 4);
        uint16_t y = (p[2] << 4) | (p[3] & 0x0F);
        if (handle->config.mirror_x && x <= handle->config.x_max) {
            x = handle->config.x_max - x;
        }
        if (handle->config.mirror_y && y <= handle->config.y_max) {
            y = handle->config.y_max - y;
        }

        points[points_num].x = x;
        points[points_num].y = y;
        points[points_num].pressure = 1;
        points[points_num].id = points_num;
        ++points_num;
    }

    return points_num;
}

esp_err_t cst9217_open(const cst9217_config_t *config, cst9217_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(config->bus != NULL, ESP_ERR_INVALID_ARG, TAG, "bus is null");

    esp_err_t ret = ESP_OK;
    cst9217_handle_t handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->config = *config;
    handle->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    const gpio_config_t reset_cfg = {
        .pin_bit_mask = BIT64(config->reset_gpio),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&reset_cfg), err, TAG, "reset gpio config failed");

    const gpio_config_t int_cfg = {
        .pin_bit_mask = BIT64(config->int_gpio),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&int_cfg), err, TAG, "int gpio config failed");

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = config->i2c_speed_hz,
    };
    ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(config->bus, &dev_cfg, &handle->dev), err, TAG, "add i2c device failed");
    ESP_GOTO_ON_ERROR(cst9217_hw_reset(handle), err, TAG, "reset failed");
    ESP_GOTO_ON_ERROR(cst9217_read_config(handle), err, TAG, "Read config failed");

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        goto err;
    }
    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(config->int_gpio, cst9217_isr, handle), err, TAG, "isr add failed");
    handle->isr_registered = true;
    ESP_GOTO_ON_ERROR(gpio_intr_enable(config->int_gpio), err, TAG, "isr enable failed");

    handle->report_pending = (gpio_get_level(config->int_gpio) == CST9217_INT_ACTIVE_LEVEL);
    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        (void)cst9217_close(handle);
    }
    return ret;
}

esp_err_t cst9217_close(cst9217_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;

    if (handle->isr_registered) {
        (void)gpio_intr_disable(handle->config.int_gpio);
        (void)gpio_isr_handler_remove(handle->config.int_gpio);
    }
    if (handle->dev != NULL) {
        first_err = i2c_master_bus_rm_device(handle->dev);
    }
    (void)gpio_reset_pin(handle->config.reset_gpio);
    (void)gpio_reset_pin(handle->config.int_gpio);
    free(handle);
    return first_err;
}

esp_err_t cst9217_read(cst9217_handle_t handle,
                       bsp_touch_point_t *points,
                       size_t points_capacity,
                       size_t *points_num)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "points is null");
    ESP_RETURN_ON_FALSE(points_num != NULL, ESP_ERR_INVALID_ARG, TAG, "points_num is null");
    ESP_RETURN_ON_FALSE(points_capacity > 0, ESP_ERR_INVALID_ARG, TAG, "points_capacity is zero");

    bool should_read = false;
    portENTER_CRITICAL(&handle->lock);
    should_read = handle->report_pending;
    handle->report_pending = false;
    portEXIT_CRITICAL(&handle->lock);

    if (!should_read) {
        cst9217_clear_stale_cached(handle);
        cst9217_copy_cached(handle, points, points_capacity, points_num);
        return ESP_OK;
    }

    uint8_t data[CST9217_DATA_LEN] = {0};
    esp_err_t ret = cst9217_read_reg(handle->dev, CST9217_DATA_REG, data, sizeof(data));
    if (ret != ESP_OK) {
        (void)cst9217_hw_reset_after_read_fail(handle);
        cst9217_clear_stale_cached(handle);
        cst9217_copy_cached(handle, points, points_capacity, points_num);
        return ret;
    }

    bsp_touch_point_t parsed_points[CST9217_MAX_POINTS] = {0};
    size_t parsed_num = cst9217_parse_points(handle, data, parsed_points, CST9217_MAX_POINTS);
    cst9217_update_cached(handle, parsed_points, parsed_num);
    cst9217_copy_cached(handle, points, points_capacity, points_num);
    return ESP_OK;
}

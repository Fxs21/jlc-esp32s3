#include "pcf85063.h"

#include <stdlib.h>

#include "esp_check.h"

#define TAG "pcf85063"

#define PCF85063_I2C_TIMEOUT_MS 1000

#define PCF85063_REG_CONTROL_1 0x00
#define PCF85063_REG_SECONDS 0x04

#define PCF85063_CTRL1_STOP  (1u << 5)
#define PCF85063_CTRL1_12_24 (1u << 1)
#define PCF85063_SECONDS_OS  (1u << 7)

struct pcf85063_t {
    i2c_master_dev_handle_t dev;
};

static esp_err_t read_regs(pcf85063_handle_t handle, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(handle->dev, &reg, sizeof(reg), buf, len, PCF85063_I2C_TIMEOUT_MS);
}

static esp_err_t write_regs(pcf85063_handle_t handle, uint8_t reg, const uint8_t *buf, size_t len)
{
    uint8_t payload[8];
    ESP_RETURN_ON_FALSE(len + 1 <= sizeof(payload), ESP_ERR_INVALID_SIZE, TAG, "payload too large");
    payload[0] = reg;
    for (size_t i = 0; i < len; i++) {
        payload[i + 1] = buf[i];
    }
    return i2c_master_transmit(handle->dev, payload, len + 1, PCF85063_I2C_TIMEOUT_MS);
}

static uint8_t dec2bcd(uint8_t value)
{
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

static uint8_t bcd2dec(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10u) + (value & 0x0Fu));
}

// 0 = Sunday .. 6 = Saturday, matches the chip weekday counter.
static uint8_t weekday_of(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t offsets[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t y = year;
    if (month < 3) {
        y--;
    }
    return (uint8_t)((y + y / 4 - y / 100 + y / 400 + offsets[month - 1] + day) % 7);
}

esp_err_t pcf85063_open(const pcf85063_config_t *cfg, pcf85063_handle_t *handle_out)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(handle_out != NULL, ESP_ERR_INVALID_ARG, TAG, "handle_out is null");
    ESP_RETURN_ON_FALSE(cfg->dev != NULL, ESP_ERR_INVALID_ARG, TAG, "i2c device is null");
    *handle_out = NULL;

    pcf85063_handle_t handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->dev = cfg->dev;

    // Probe the chip and force 24-hour mode, which is what the BSP time struct assumes.
    uint8_t ctrl1 = 0;
    esp_err_t ret = read_regs(handle, PCF85063_REG_CONTROL_1, &ctrl1, sizeof(ctrl1));
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }
    const uint8_t wanted = (uint8_t)(ctrl1 & ~(PCF85063_CTRL1_STOP | PCF85063_CTRL1_12_24));
    if (wanted != ctrl1) {
        ret = write_regs(handle, PCF85063_REG_CONTROL_1, &wanted, sizeof(wanted));
        if (ret != ESP_OK) {
            free(handle);
            return ret;
        }
    }

    *handle_out = handle;
    return ESP_OK;
}

esp_err_t pcf85063_close(pcf85063_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    free(handle);
    return ESP_OK;
}

esp_err_t pcf85063_read_time(pcf85063_handle_t handle, pcf85063_time_t *time_out, bool *valid_out)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(time_out != NULL, ESP_ERR_INVALID_ARG, TAG, "time_out is null");
    ESP_RETURN_ON_FALSE(valid_out != NULL, ESP_ERR_INVALID_ARG, TAG, "valid_out is null");

    // 04h..0Ah (seconds..years) in one burst keeps the fields consistent.
    uint8_t regs[7] = {0};
    ESP_RETURN_ON_ERROR(read_regs(handle, PCF85063_REG_SECONDS, regs, sizeof(regs)), TAG, "read time failed");

    *valid_out = (regs[0] & PCF85063_SECONDS_OS) == 0;
    time_out->second = bcd2dec(regs[0] & 0x7F);
    time_out->minute = bcd2dec(regs[1] & 0x7F);
    time_out->hour = bcd2dec(regs[2] & 0x3F);
    time_out->day = bcd2dec(regs[3] & 0x3F);
    time_out->month = bcd2dec(regs[5] & 0x1F);
    time_out->year = (uint16_t)(2000u + bcd2dec(regs[6]));
    return ESP_OK;
}

esp_err_t pcf85063_write_time(pcf85063_handle_t handle, const pcf85063_time_t *time)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(time != NULL, ESP_ERR_INVALID_ARG, TAG, "time is null");

    uint8_t regs[7];
    regs[0] = dec2bcd(time->second); // bit 7 = 0 clears the OS flag
    regs[1] = dec2bcd(time->minute);
    regs[2] = dec2bcd(time->hour);
    regs[3] = dec2bcd(time->day);
    regs[4] = weekday_of(time->year, time->month, time->day);
    regs[5] = dec2bcd(time->month);
    regs[6] = dec2bcd((uint8_t)(time->year - 2000u));
    return write_regs(handle, PCF85063_REG_SECONDS, regs, sizeof(regs));
}

#include "bsp_rtc.h"

#include <stdlib.h>

#include "auras3_pins.h"
#include "bsp_i2c.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "pcf85063.h"

#define TAG "bsp_rtc"

struct bsp_rtc_t {
    i2c_master_dev_handle_t dev;
    pcf85063_handle_t chip;
    bool i2c_acquired;
};

static bool s_rtc_open;

static const bsp_rtc_desc_t s_desc = {
    .present = true,
};

static esp_err_t close_internal(bsp_rtc_handle_t rtc)
{
    esp_err_t first_err = ESP_OK;
    if (rtc->chip != NULL) {
        esp_err_t ret = pcf85063_close(rtc->chip);
        rtc->chip = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (rtc->dev != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(rtc->dev);
        rtc->dev = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (rtc->i2c_acquired) {
        esp_err_t ret = bsp_i2c_release();
        rtc->i2c_acquired = false;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    return first_err;
}

const bsp_rtc_desc_t *bsp_rtc_get_desc(void)
{
    return &s_desc;
}

static bool time_is_sane(const bsp_rtc_time_t *time)
{
    return time->year >= 2000u && time->year <= 2099u &&
           time->month >= 1u && time->month <= 12u &&
           time->day >= 1u && time->day <= 31u &&
           time->hour <= 23u && time->minute <= 59u && time->second <= 59u;
}

esp_err_t bsp_rtc_open(bsp_rtc_handle_t *rtc_out)
{
    ESP_RETURN_ON_FALSE(rtc_out != NULL, ESP_ERR_INVALID_ARG, TAG, "rtc_out is null");
    ESP_RETURN_ON_FALSE(!s_rtc_open, ESP_ERR_INVALID_STATE, TAG, "rtc already open");
    *rtc_out = NULL;

    bsp_rtc_handle_t rtc = calloc(1, sizeof(*rtc));
    ESP_RETURN_ON_FALSE(rtc != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    i2c_master_bus_handle_t bus = NULL;
    esp_err_t ret = bsp_i2c_acquire(&bus);
    if (ret != ESP_OK) {
        goto err;
    }
    rtc->i2c_acquired = true;

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AURAS3_RTC_I2C_ADDR,
        .scl_speed_hz = AURAS3_RTC_SPEED_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &rtc->dev);
    if (ret != ESP_OK) {
        goto err;
    }

    const pcf85063_config_t chip_cfg = {
        .dev = rtc->dev,
    };
    ret = pcf85063_open(&chip_cfg, &rtc->chip);
    if (ret != ESP_OK) {
        goto err;
    }

    s_rtc_open = true;
    *rtc_out = rtc;
    return ESP_OK;

err:
    (void)close_internal(rtc);
    free(rtc);
    return ret;
}

esp_err_t bsp_rtc_close(bsp_rtc_handle_t rtc)
{
    ESP_RETURN_ON_FALSE(rtc != NULL, ESP_ERR_INVALID_ARG, TAG, "rtc is null");
    s_rtc_open = false;
    esp_err_t ret = close_internal(rtc);
    free(rtc);
    return ret;
}

esp_err_t bsp_rtc_get_time(bsp_rtc_handle_t rtc, bsp_rtc_time_t *time_out, bool *valid_out)
{
    ESP_RETURN_ON_FALSE(rtc != NULL, ESP_ERR_INVALID_ARG, TAG, "rtc is null");
    ESP_RETURN_ON_FALSE(time_out != NULL, ESP_ERR_INVALID_ARG, TAG, "time_out is null");
    ESP_RETURN_ON_FALSE(valid_out != NULL, ESP_ERR_INVALID_ARG, TAG, "valid_out is null");

    pcf85063_time_t time = {0};
    bool valid = false;
    ESP_RETURN_ON_ERROR(pcf85063_read_time(rtc->chip, &time, &valid), TAG, "read time failed");

    time_out->year = time.year;
    time_out->month = time.month;
    time_out->day = time.day;
    time_out->hour = time.hour;
    time_out->minute = time.minute;
    time_out->second = time.second;
    *valid_out = valid;
    return ESP_OK;
}

esp_err_t bsp_rtc_set_time(bsp_rtc_handle_t rtc, const bsp_rtc_time_t *time)
{
    ESP_RETURN_ON_FALSE(rtc != NULL, ESP_ERR_INVALID_ARG, TAG, "rtc is null");
    ESP_RETURN_ON_FALSE(time != NULL, ESP_ERR_INVALID_ARG, TAG, "time is null");
    ESP_RETURN_ON_FALSE(time_is_sane(time), ESP_ERR_INVALID_ARG, TAG, "time fields out of range");

    const pcf85063_time_t chip_time = {
        .year = time->year,
        .month = time->month,
        .day = time->day,
        .hour = time->hour,
        .minute = time->minute,
        .second = time->second,
    };
    ESP_RETURN_ON_ERROR(pcf85063_write_time(rtc->chip, &chip_time), TAG, "write time failed");
    return ESP_OK;
}

#include "backlight_ledc.h"

#include <stdlib.h>

#include "driver/ledc.h"
#include "esp_check.h"

#define TAG "backlight_ledc"
#define BL_LEDC_CHANNEL    LEDC_CHANNEL_0
#define BL_LEDC_TIMER      LEDC_TIMER_0
#define BL_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define BL_PWM_FREQ_HZ     5000
#define BL_DUTY_RESOLUTION LEDC_TIMER_13_BIT

struct backlight_ledc_s {
    gpio_num_t gpio;
    bool output_invert;
};

static esp_err_t off(void)
{
    return ledc_stop(BL_LEDC_MODE, BL_LEDC_CHANNEL, 0);
}

esp_err_t backlight_ledc_open(const backlight_ledc_config_t *cfg, backlight_ledc_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    struct backlight_ledc_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->gpio = cfg->gpio;
    handle->output_invert = cfg->output_invert;

    ledc_timer_config_t timer_cfg = {
        .speed_mode = BL_LEDC_MODE,
        .timer_num = BL_LEDC_TIMER,
        .duty_resolution = BL_DUTY_RESOLUTION,
        .freq_hz = BL_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    ledc_channel_config_t channel_cfg = {
        .speed_mode = BL_LEDC_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = handle->gpio,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = handle->output_invert,
        },
    };
    ret = ledc_channel_config(&channel_cfg);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }
    ret = off();
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t backlight_ledc_close(backlight_ledc_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    (void)off();
    free(handle);
    return ESP_OK;
}

esp_err_t backlight_ledc_set_percent(backlight_ledc_handle_t handle, uint8_t percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    if (percent > 100) {
        percent = 100;
    }
    if (percent == 0) {
        return off();
    }
    uint32_t duty_max = (1u << BL_DUTY_RESOLUTION) - 1u;
    uint32_t duty = (uint32_t)percent * duty_max / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL), TAG, "update duty failed");
    return ESP_OK;
}

#include "bsp_key.h"

#include <stdlib.h>

#include "doers3_pins.h"
#include "driver/gpio.h"
#include "esp_check.h"

#define TAG "bsp_key"

struct bsp_key_s {
    gpio_num_t gpio;
};

static bool s_key_open;

static const bsp_key_desc_t s_desc = {
    .present = true,
};

const bsp_key_desc_t *bsp_key_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_key_open(bsp_key_handle_t *handle_out)
{
    ESP_RETURN_ON_FALSE(handle_out != NULL, ESP_ERR_INVALID_ARG, TAG, "handle_out is null");
    ESP_RETURN_ON_FALSE(!s_key_open, ESP_ERR_INVALID_STATE, TAG, "key already open");
    *handle_out = NULL;

    struct bsp_key_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << DOERS3_KEY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&cfg);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    handle->gpio = DOERS3_KEY_GPIO;
    s_key_open = true;
    *handle_out = handle;
    return ESP_OK;
}

esp_err_t bsp_key_close(bsp_key_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    s_key_open = false;
    free(handle);
    return ESP_OK;
}

esp_err_t bsp_key_is_pressed(bsp_key_handle_t handle, bool *pressed_out)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(pressed_out != NULL, ESP_ERR_INVALID_ARG, TAG, "pressed_out is null");
    *pressed_out = gpio_get_level(handle->gpio) == DOERS3_KEY_ACTIVE_LEVEL;
    return ESP_OK;
}

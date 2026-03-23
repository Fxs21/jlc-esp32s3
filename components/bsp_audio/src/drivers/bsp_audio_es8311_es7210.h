#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <stddef.h>

#include "bsp_audio.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_AUDIO_CTRL_PIN_NONE = 0,
    BSP_AUDIO_CTRL_PIN_GPIO,
    BSP_AUDIO_CTRL_PIN_IOEXP,
} bsp_audio_ctrl_pin_type_t;

typedef struct {
    bsp_audio_ctrl_pin_type_t type;
    bool active_high;
    union {
        gpio_num_t gpio_num;
        uint8_t ioexp_num;
    } num;
} bsp_audio_ctrl_pin_t;

typedef struct {
    i2s_port_t i2s_port;
    gpio_num_t mclk;
    gpio_num_t bclk;
    gpio_num_t ws;
    gpio_num_t dout;
    gpio_num_t din;
    uint8_t es8311_addr;
    uint8_t es7210_addr;
    uint8_t es7210_mic_mask;
    bsp_audio_ctrl_pin_t pa_enable;
    bool use_mclk;
    uint16_t mclk_multiple;
} bsp_audio_es8311_es7210_cfg_t;

typedef struct bsp_audio_driver_s *bsp_audio_driver_handle_t;

esp_err_t bsp_audio_es8311_es7210_new(const bsp_audio_es8311_es7210_cfg_t *config,
                                      bsp_audio_driver_handle_t *out_handle);
esp_err_t bsp_audio_es8311_es7210_del(bsp_audio_driver_handle_t handle);
esp_err_t bsp_audio_es8311_es7210_start(bsp_audio_driver_handle_t handle,
                                        const bsp_audio_stream_cfg_t *stream_cfg);
esp_err_t bsp_audio_es8311_es7210_stop(bsp_audio_driver_handle_t handle);
esp_err_t bsp_audio_es8311_es7210_set_out_vol(bsp_audio_driver_handle_t handle, int volume);
esp_err_t bsp_audio_es8311_es7210_set_in_gain(bsp_audio_driver_handle_t handle, float gain_db);
esp_err_t bsp_audio_es8311_es7210_write(bsp_audio_driver_handle_t handle, void *data, size_t len);
esp_err_t bsp_audio_es8311_es7210_read(bsp_audio_driver_handle_t handle, void *data, size_t len);

#ifdef __cplusplus
}
#endif

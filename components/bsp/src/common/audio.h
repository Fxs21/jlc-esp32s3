#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp_audio.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_types.h"
#include "es8311.h"
#include "es7210.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_audio_common bsp_audio_common_t;

typedef struct {
    i2s_port_t  port;
    gpio_num_t  mclk;
    gpio_num_t  bclk;
    gpio_num_t  ws;
    gpio_num_t  dout;
    gpio_num_t  din;
    uint8_t     es8311_addr;
    uint8_t     es7210_addr;
} bsp_audio_pins_t;

// PA control callback. Called with true=on, false=off. NULL if no PA.
typedef esp_err_t (*bsp_audio_pa_fn)(bool on);

// Create/destroy. Caller owns the lifecycle.
bsp_audio_common_t *bsp_audio_common_create(void);
void bsp_audio_common_destroy(bsp_audio_common_t *c);

// Init I2S channels and codecs. bus is the already-acquired I2C master bus.
esp_err_t bsp_audio_common_init(bsp_audio_common_t *c,
                                i2c_master_bus_handle_t bus,
                                const bsp_audio_pins_t *pins,
                                const bsp_audio_config_t *config);
void bsp_audio_common_deinit(bsp_audio_common_t *c);

// Playback. pa may be NULL.
esp_err_t bsp_audio_common_play_start(bsp_audio_common_t *c, bsp_audio_pa_fn pa);
esp_err_t bsp_audio_common_play_stop(bsp_audio_common_t *c, bsp_audio_pa_fn pa);
esp_err_t bsp_audio_common_play_set_volume(bsp_audio_common_t *c, int volume);
esp_err_t bsp_audio_common_play_write(bsp_audio_common_t *c,
                                      const void *data, size_t len,
                                      size_t *out_written, uint32_t timeout_ms);

// Record.
esp_err_t bsp_audio_common_record_start(bsp_audio_common_t *c);
esp_err_t bsp_audio_common_record_stop(bsp_audio_common_t *c);
esp_err_t bsp_audio_common_record_set_gain(bsp_audio_common_t *c, float gain_db);
esp_err_t bsp_audio_common_record_read(bsp_audio_common_t *c,
                                       void *data, size_t len,
                                       size_t *out_read, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

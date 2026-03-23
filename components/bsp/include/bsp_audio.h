#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_audio_s *bsp_audio_handle_t;

typedef enum {
    BSP_AUDIO_SAMPLE_FORMAT_S16_LE = 0,
} bsp_audio_sample_format_t;

typedef struct {
    bool present;
    bool has_playback;
    bool has_record;
    bool supports_full_duplex;
    bool shared_clock;
} bsp_audio_desc_t;

typedef struct {
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    bsp_audio_sample_format_t sample_format;
} bsp_audio_config_t;

bsp_audio_desc_t const *bsp_audio_get_desc(void);
bsp_audio_config_t bsp_audio_default_config(void);

// On failure, *out_handle is set to NULL after out_handle is validated.
esp_err_t bsp_audio_open(const bsp_audio_config_t *config, bsp_audio_handle_t *out_handle);
esp_err_t bsp_audio_close(bsp_audio_handle_t handle);

esp_err_t bsp_audio_play_start(bsp_audio_handle_t handle);
esp_err_t bsp_audio_play_stop(bsp_audio_handle_t handle);
// Playback volume percentage. Values are clamped to the codec-supported 0..100 range.
esp_err_t bsp_audio_play_set_volume(bsp_audio_handle_t handle, int volume);
esp_err_t bsp_audio_play_write(bsp_audio_handle_t handle,
                               const void *data,
                               size_t len,
                               size_t *out_written,
                               uint32_t timeout_ms);

esp_err_t bsp_audio_record_start(bsp_audio_handle_t handle);
esp_err_t bsp_audio_record_stop(bsp_audio_handle_t handle);
// Record gain in dB. Unsupported values are clamped by the board codec driver.
esp_err_t bsp_audio_record_set_gain(bsp_audio_handle_t handle, float gain_db);
esp_err_t bsp_audio_record_read(bsp_audio_handle_t handle,
                                void *data,
                                size_t len,
                                size_t *out_read,
                                uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

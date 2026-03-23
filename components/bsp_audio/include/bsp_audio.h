#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_audio_s *bsp_audio_handle_t;

typedef struct {
    bool present;
    bool has_playback;
    bool has_record;
} bsp_audio_desc_t;

typedef enum {
    BSP_AUDIO_MODE_NONE = 0,
    BSP_AUDIO_MODE_PLAYBACK = (1 << 0),
    BSP_AUDIO_MODE_RECORD = (1 << 1),
    BSP_AUDIO_MODE_DUPLEX = BSP_AUDIO_MODE_PLAYBACK | BSP_AUDIO_MODE_RECORD,
} bsp_audio_mode_t;

typedef struct {
    bsp_audio_mode_t mode;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
} bsp_audio_stream_cfg_t;

bsp_audio_desc_t const *bsp_audio_get_desc(void);
bsp_audio_stream_cfg_t bsp_audio_default_stream_config(void);

esp_err_t bsp_audio_new(bsp_audio_handle_t *out_handle);
esp_err_t bsp_audio_del(bsp_audio_handle_t handle);

esp_err_t bsp_audio_start(bsp_audio_handle_t handle, const bsp_audio_stream_cfg_t *stream_cfg);
esp_err_t bsp_audio_stop(bsp_audio_handle_t handle);

esp_err_t bsp_audio_set_out_vol(bsp_audio_handle_t handle, int volume);
esp_err_t bsp_audio_set_in_gain(bsp_audio_handle_t handle, float gain_db);

esp_err_t bsp_audio_write(bsp_audio_handle_t handle, void *data, size_t len);
esp_err_t bsp_audio_read(bsp_audio_handle_t handle, void *data, size_t len);

#ifdef __cplusplus
}
#endif

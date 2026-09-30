#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// One audio session: a shared I2S clock domain, one format, and an optional
// record path. Playback and record are two independent streams inside the
// session; they may run at the same time (full-duplex).
typedef struct bsp_audio_s *bsp_audio_handle_t;

// Record channel mask. The recorded stream interleaves one sample per set bit,
// in bit order, and the bit order is the ES7210 TDM slot order:
// slot 0 = MIC1, slot 1 = LOOPBACK (ES7210 MIC3), slot 2 = MIC2.
// The frame size is popcount(mask) * (bits_per_sample / 8).
// LOOPBACK is the ES8311 playback output fed back into ES7210 MIC3 and used as
// the AEC reference; it is not a third microphone.
typedef enum {
    BSP_AUDIO_RECORD_CH_MIC1     = 1u << 0,
    BSP_AUDIO_RECORD_CH_LOOPBACK = 1u << 1,
    BSP_AUDIO_RECORD_CH_MIC2     = 1u << 2,
} bsp_audio_record_channel_t;

// Session configurations accepted by bsp_audio_open():
//   record_channel_mask = 0                    playback only
//   MIC1 | MIC2                               16-bit stereo record (I2S)
//   MIC1 | MIC2 | LOOPBACK                    16-bit 3-channel record (TDM)
// Other masks return ESP_ERR_NOT_SUPPORTED.
//
// Supported sample rates are the standard set the codec drivers carry
// coefficients for: 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100,
// 48000 Hz. Other rates return ESP_ERR_NOT_SUPPORTED.
typedef struct {
    uint32_t sample_rate;
    uint8_t bits_per_sample;      // 16 only
    uint8_t record_channel_mask;  // 0 = playback only
} bsp_audio_config_t;

typedef struct {
    bool present;
    bool has_playback;
    bool has_record;
    bool has_loopback;          // LOOPBACK usable on this board
    bool supports_full_duplex;  // playback + record at once, verified on device
    bool shared_clock;          // playback and record share one I2S clock domain
    uint32_t sample_rate_min;
    uint32_t sample_rate_max;
} bsp_audio_desc_t;

bsp_audio_desc_t const *bsp_audio_get_desc(void);
bsp_audio_config_t bsp_audio_default_config(void);

// On failure, *handle_out is set to NULL after handle_out is validated.
esp_err_t bsp_audio_open(const bsp_audio_config_t *config, bsp_audio_handle_t *handle_out);
esp_err_t bsp_audio_close(bsp_audio_handle_t handle);

// Playback takes mono frames; the BSP feeds the codec with the left channel.
// Record delivers interleaved frames as described by record_channel_mask.
// read/write block for at most timeout_ms and report the transferred bytes;
// len must be a multiple of the frame size.
esp_err_t bsp_audio_play_start(bsp_audio_handle_t handle);
esp_err_t bsp_audio_play_stop(bsp_audio_handle_t handle);
// Playback volume percentage, 0..100, mapped to the codec volume range.
esp_err_t bsp_audio_play_set_volume(bsp_audio_handle_t handle, int volume);
esp_err_t bsp_audio_play_set_mute(bsp_audio_handle_t handle, bool mute);
esp_err_t bsp_audio_play_write(bsp_audio_handle_t handle,
                               const void *data,
                               size_t len,
                               size_t *written_out,
                               uint32_t timeout_ms);

esp_err_t bsp_audio_record_start(bsp_audio_handle_t handle);
esp_err_t bsp_audio_record_stop(bsp_audio_handle_t handle);
// Record gain in dB, clamped by the board codec driver. channel_mask must be a
// subset of the session's record_channel_mask.
esp_err_t bsp_audio_record_set_gain(bsp_audio_handle_t handle, uint32_t channel_mask, float gain_db);
esp_err_t bsp_audio_record_read(bsp_audio_handle_t handle,
                                void *data,
                                size_t len,
                                size_t *read_out,
                                uint32_t timeout_ms);

// Threading: play_* and record_* may be driven from two different tasks at the
// same time. Calls into the same direction are serialized by the caller.

#ifdef __cplusplus
}
#endif

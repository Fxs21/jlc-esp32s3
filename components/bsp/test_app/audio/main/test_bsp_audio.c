#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bsp_audio.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "test_bsp_audio"
#define TONE_HZ 440
#define TONE_MS 800
#define TONE_AMPLITUDE 1200
#define FRAME_SAMPLES 256
#define RECORD_ROUNDS 20
#define PI_F 3.14159265358979323846f

static void fill_tone(int16_t *samples, size_t frames, uint32_t sample_rate, uint8_t channels, uint32_t *phase)
{
    for (size_t i = 0; i < frames; ++i) {
        float t = (float)(*phase) / (float)sample_rate;
        int16_t v = (int16_t)(sinf(2.0f * PI_F * (float)TONE_HZ * t) * TONE_AMPLITUDE);
        (*phase)++;
        for (uint8_t ch = 0; ch < channels; ++ch) {
            samples[i * channels + ch] = v;
        }
    }
}

static void sample_min_max(const int16_t *samples, size_t count, int16_t *out_min, int16_t *out_max)
{
    int16_t min = samples[0];
    int16_t max = samples[0];
    for (size_t i = 1; i < count; ++i) {
        if (samples[i] < min) {
            min = samples[i];
        }
        if (samples[i] > max) {
            max = samples[i];
        }
    }
    *out_min = min;
    *out_max = max;
}

void app_main(void)
{
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if (!desc->present) {
        ESP_LOGW(TAG, "audio not present on this board");
        return;
    }

    bsp_audio_handle_t audio = NULL;
    ESP_ERROR_CHECK(bsp_audio_open(&audio));

    bsp_audio_stream_cfg_t cfg = bsp_audio_default_stream_config();
    ESP_ERROR_CHECK(bsp_audio_start(audio, &cfg));
    ESP_ERROR_CHECK(bsp_audio_set_out_vol(audio, 35));
    ESP_ERROR_CHECK(bsp_audio_set_in_gain(audio, 30.0f));

    size_t sample_count = FRAME_SAMPLES * cfg.channels;
    size_t bytes = sample_count * sizeof(int16_t);
    int16_t *buf = calloc(sample_count, sizeof(int16_t));
    if (buf == NULL) {
        ESP_LOGE(TAG, "no memory for audio buffer");
        ESP_ERROR_CHECK(bsp_audio_stop(audio));
        ESP_ERROR_CHECK(bsp_audio_close(audio));
        return;
    }

    ESP_LOGI(TAG, "play %dHz tone for %dms", TONE_HZ, TONE_MS);
    uint32_t phase = 0;
    uint32_t frames_total = cfg.sample_rate * TONE_MS / 1000;
    for (uint32_t done = 0; done < frames_total; done += FRAME_SAMPLES) {
        size_t frames = frames_total - done;
        if (frames > FRAME_SAMPLES) {
            frames = FRAME_SAMPLES;
        }
        fill_tone(buf, frames, cfg.sample_rate, cfg.channels, &phase);
        ESP_ERROR_CHECK(bsp_audio_write(audio, buf, frames * cfg.channels * sizeof(int16_t)));
    }

    ESP_LOGI(TAG, "record sample ranges");
    for (int i = 0; i < RECORD_ROUNDS; ++i) {
        ESP_ERROR_CHECK(bsp_audio_read(audio, buf, bytes));
        int16_t min = 0;
        int16_t max = 0;
        sample_min_max(buf, sample_count, &min, &max);
        printf("audio range: min=%d max=%d span=%d\n", min, max, max - min);
    }

    free(buf);
    ESP_ERROR_CHECK(bsp_audio_stop(audio));
    ESP_ERROR_CHECK(bsp_audio_close(audio));
}

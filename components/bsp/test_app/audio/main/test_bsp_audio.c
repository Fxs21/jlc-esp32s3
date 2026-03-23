#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsp_audio.h"
#include "esp_check.h"
#include "esp_log.h"

#define TAG "test_bsp_audio"
#define TONE_HZ 1000
#define PLAY_MS 2000
#define TONE_AMPLITUDE 8000
#define PLAY_VOLUME 60
#define RECORD_ROUNDS 20
#define FRAME_SAMPLES 256
#define MIN_RECORD_RMS 1.0
#define PI_F 3.14159265358979323846f

struct channel_stats {
    double sum_sq[2];
    int16_t min[2];
    int16_t max[2];
    int16_t peak[2];
    size_t count[2];
};

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

static void stats_init(struct channel_stats *stats)
{
    for (uint8_t ch = 0; ch < 2; ++ch) {
        stats->sum_sq[ch] = 0.0;
        stats->min[ch] = INT16_MAX;
        stats->max[ch] = INT16_MIN;
        stats->peak[ch] = 0;
        stats->count[ch] = 0;
    }
}

static void stats_add(struct channel_stats *stats, const int16_t *samples, size_t frames)
{
    for (size_t i = 0; i < frames; ++i) {
        for (uint8_t ch = 0; ch < 2; ++ch) {
            int16_t sample = samples[i * 2 + ch];
            int32_t abs_sample = sample < 0 ? -(int32_t)sample : sample;
            if (sample < stats->min[ch]) {
                stats->min[ch] = sample;
            }
            if (sample > stats->max[ch]) {
                stats->max[ch] = sample;
            }
            if (abs_sample > stats->peak[ch]) {
                stats->peak[ch] = (int16_t)abs_sample;
            }
            stats->sum_sq[ch] += (double)sample * (double)sample;
            stats->count[ch]++;
        }
    }
}

static double stats_rms(const struct channel_stats *stats, uint8_t ch)
{
    if (stats->count[ch] == 0) {
        return 0.0;
    }
    return sqrt(stats->sum_sq[ch] / (double)stats->count[ch]);
}

static esp_err_t run_playback_test(void)
{
    bsp_audio_config_t cfg = bsp_audio_default_config();
    bsp_audio_handle_t audio = NULL;
    ESP_RETURN_ON_ERROR(bsp_audio_open(&cfg, &audio), TAG, "open playback session failed");

    int16_t *buf = calloc(FRAME_SAMPLES * 2, sizeof(int16_t));
    if (buf == NULL) {
        (void)bsp_audio_close(audio);
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(bsp_audio_play_start(audio), TAG, "start playback failed");
    ESP_RETURN_ON_ERROR(bsp_audio_play_set_volume(audio, PLAY_VOLUME), TAG, "set volume failed");

    ESP_LOGI(TAG, "playback: play %dHz tone for %dms, volume=%d", TONE_HZ, PLAY_MS, PLAY_VOLUME);
    uint32_t phase = 0;
    uint32_t frames_total = cfg.sample_rate * PLAY_MS / 1000;
    for (uint32_t done = 0; done < frames_total; done += FRAME_SAMPLES) {
        size_t frames = frames_total - done;
        if (frames > FRAME_SAMPLES) {
            frames = FRAME_SAMPLES;
        }
        fill_tone(buf, frames, cfg.sample_rate, 2, &phase);
        size_t bytes = frames * 2 * sizeof(int16_t);
        size_t written = 0;
        ESP_RETURN_ON_ERROR(bsp_audio_play_write(audio, buf, bytes, &written, UINT32_MAX), TAG, "play tone failed");
        ESP_RETURN_ON_FALSE(written == bytes, ESP_ERR_TIMEOUT, TAG, "short playback write");
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_audio_play_stop(audio));
    free(buf);
    return bsp_audio_close(audio);
}

static esp_err_t run_record_test(void)
{
    bsp_audio_config_t cfg = bsp_audio_default_config();
    bsp_audio_handle_t audio = NULL;
    ESP_RETURN_ON_ERROR(bsp_audio_open(&cfg, &audio), TAG, "open record session failed");

    int16_t *buf = calloc(FRAME_SAMPLES * 2, sizeof(int16_t));
    if (buf == NULL) {
        (void)bsp_audio_close(audio);
        return ESP_ERR_NO_MEM;
    }

    struct channel_stats stats;
    stats_init(&stats);

    ESP_RETURN_ON_ERROR(bsp_audio_record_start(audio), TAG, "start record failed");
    ESP_RETURN_ON_ERROR(bsp_audio_record_set_gain(audio, 30.0f), TAG, "set record gain failed");

    ESP_LOGI(TAG, "record: speak near MIC1/MIC2 to see rms/peak/span increase");
    for (int i = 0; i < RECORD_ROUNDS; ++i) {
        size_t bytes = FRAME_SAMPLES * 2 * sizeof(int16_t);
        size_t bytes_read = 0;
        ESP_RETURN_ON_ERROR(bsp_audio_record_read(audio, buf, bytes, &bytes_read, UINT32_MAX), TAG,
                            "record read failed");
        ESP_RETURN_ON_FALSE(bytes_read % (2 * sizeof(int16_t)) == 0,
                            ESP_FAIL, TAG, "unaligned record read");
        stats_add(&stats, buf, bytes_read / (2 * sizeof(int16_t)));
    }

    ESP_RETURN_ON_ERROR(bsp_audio_record_stop(audio), TAG, "stop record failed");
    ESP_RETURN_ON_ERROR(bsp_audio_close(audio), TAG, "close record session failed");

    double ch0_rms = stats_rms(&stats, 0);
    double ch1_rms = stats_rms(&stats, 1);
    ESP_LOGI(TAG, "mic ch0: rms=%.1f peak=%d min=%d max=%d span=%d", ch0_rms, stats.peak[0], stats.min[0],
             stats.max[0], stats.max[0] - stats.min[0]);
    ESP_LOGI(TAG, "mic ch1: rms=%.1f peak=%d min=%d max=%d span=%d", ch1_rms, stats.peak[1], stats.min[1],
             stats.max[1], stats.max[1] - stats.min[1]);

    free(buf);
    ESP_RETURN_ON_FALSE(ch0_rms >= MIN_RECORD_RMS || ch1_rms >= MIN_RECORD_RMS, ESP_FAIL, TAG,
                        "record data looks silent");
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "TEST START name=audio");
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if (!desc->present) {
        ESP_LOGW(TAG, "TEST SKIP name=audio reason=\"not present\"");
        return;
    }

    ESP_LOGI(TAG, "audio sanity test");
    esp_err_t ret = run_playback_test();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=audio step=playback err=%s", esp_err_to_name(ret));
        return;
    }
    ret = run_record_test();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=audio step=record err=%s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "TEST PASS name=audio summary=\"playback and stereo record sanity passed\"");
}

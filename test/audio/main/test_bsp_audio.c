// test/audio: bsp_audio 板级自检. 只使用 BSP public API; 判据是本文件的常量.
// 覆盖: desc, open/close, 参数校验, 音量/静音, start/stop 幂等, 播放, 回采 (含 full-duplex),
// 双麦拾音和非默认采样率. 回采是全双工路径的判据: 播放和录音同时跑, 且录到播放的 1 kHz 音.
// 喇叭出声由人工项确认: 回采只证明 codec 环回, 不证明功放和喇叭.

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp_audio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "audio"

static const char *TAG = "test_bsp_audio";

// 音源和判据. 数值是测试标准的一部分, 改动等于改标准.
#define TONE_HZ                 1000
#define TONE_AMPLITUDE          12000
#define PLAY_VOLUME             80
#define RECORD_GAIN_DB          30.0f
// 回采是 ES8311 的 line 级输出, 走 30 dB 录音增益会削顶, 这里用 0 dB.
#define LOOPBACK_GAIN_DB        0.0f
#define IO_TIMEOUT_MS           1000
#define TONE_PLAY_MS            2000
#define TONE_CAPTURE_MS         1000
#define MIC_CAPTURE_MS          3000
#define CHUNK_FRAMES            512
#define MAX_RECORD_CHANNELS     3
// 3 通道流里回采 (ES7210 MIC3) 的下标, 等于 BSP_AUDIO_RECORD_CH_LOOPBACK 的 bit 位.
#define LOOPBACK_CHANNEL        1
// 播放前的静音基线: 用来证明后面看到的信号确实来自播放, 而不是常驻干扰.
#define SILENCE_CAPTURE_MS      400
// 诊断用频谱扫描: 只有回采出问题时才会有人看这些行.
#define ANALYSIS_FRAMES         4096
#define SCAN_MIN_HZ             100
// 扫到接近 Nyquist: 回采出问题时, 主频可能被搬移 (例如播放速率不匹配), 不能只看 1 kHz 附近.
#define SCAN_MAX_HZ             7800
#define SCAN_STEP_HZ            25
// 回采通道的 1 kHz 能量占比下限: 纯音约 0.5, 底噪在 1e-3 量级.
#define LOOPBACK_MIN_TONE_RATIO 0.10
// 回采通道 RMS 下限 (16-bit LSB): 播放音量 80, 回采增益 0 dB.
#define LOOPBACK_MIN_RMS        50.0
// 对着麦克风讲话/敲击时的 RMS 下限; 安静时远低于该值.
#define MIC_MIN_RMS             50.0
#define HUMAN_TIMEOUT_MS        30000

struct channel_stats {
    double sum_sq;
    double tone;      // 每块 1 kHz 的 Goertzel 功率之和
    double tone_ref;  // Σ n_i^2, 把分块功率折算回整段功率
    int16_t peak;
    size_t frames;
};

static bsp_audio_handle_t s_audio;
static uint32_t s_phase;

static struct channel_stats s_stats[MAX_RECORD_CHANNELS];

static int16_t s_play_chunk[CHUNK_FRAMES];
static int16_t s_rec_chunk[CHUNK_FRAMES * MAX_RECORD_CHANNELS];

static int16_t s_analysis[ANALYSIS_FRAMES * MAX_RECORD_CHANNELS];
static size_t s_analysis_frames;
static size_t s_analysis_channels;

void setUp(void)
{
}

void tearDown(void)
{
    if (s_audio != NULL) {
        (void)bsp_audio_close(s_audio);
        s_audio = NULL;
    }
}

static void open_audio(const bsp_audio_config_t *config)
{
    s_audio = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_open(config, &s_audio));
    TEST_ASSERT_NOT_NULL(s_audio);
}

static size_t record_channels(uint8_t mask)
{
    size_t channels = 0;
    for (uint8_t i = 0; i < 3; ++i) {
        if ((mask & (1u << i)) != 0) {
            channels++;
        }
    }
    return channels;
}

static void fill_tone(int16_t *dst, size_t frames, uint32_t sample_rate)
{
    for (size_t i = 0; i < frames; ++i) {
        const float t = (float)s_phase / (float)sample_rate;
        dst[i] = (int16_t)lrintf(sinf(2.0f * (float)M_PI * (float)TONE_HZ * t) * (float)TONE_AMPLITUDE);
        s_phase++;
    }
}

static void stats_reset(size_t channels)
{
    for (size_t ch = 0; ch < channels; ++ch) {
        s_stats[ch].sum_sq = 0.0;
        s_stats[ch].tone = 0.0;
        s_stats[ch].tone_ref = 0.0;
        s_stats[ch].peak = 0;
        s_stats[ch].frames = 0;
    }
}

// 每块先做 Goertzel 再累加: 块内累计器初值必须为零, 所以不能跨块共用状态.
static void stats_add(const int16_t *frames, size_t frame_count, size_t channels, uint32_t sample_rate)
{
    const double coeff = 2.0 * cos(2.0 * M_PI * (double)TONE_HZ / (double)sample_rate);

    for (size_t ch = 0; ch < channels; ++ch) {
        double s1 = 0.0;
        double s2 = 0.0;
        for (size_t i = 0; i < frame_count; ++i) {
            const int16_t sample = frames[i * channels + ch];
            const double s0 = (double)sample + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;

            const double value = (double)sample;
            s_stats[ch].sum_sq += value * value;
            const int16_t magnitude = sample < 0 ? (int16_t)-sample : sample;
            if (magnitude > s_stats[ch].peak) {
                s_stats[ch].peak = magnitude;
            }
        }
        s_stats[ch].tone += s1 * s1 + s2 * s2 - coeff * s1 * s2;
        s_stats[ch].tone_ref += (double)frame_count * (double)frame_count;
        s_stats[ch].frames += frame_count;
    }
}

static double stats_rms(size_t ch)
{
    const struct channel_stats *stats = &s_stats[ch];
    return stats->frames == 0 ? 0.0 : sqrt(stats->sum_sq / (double)stats->frames);
}

// |X(f)|^2 / (N^2 * mean_square): 纯音约 0.5, 宽带噪声远小于 1.
static double stats_tone_ratio(size_t ch)
{
    const struct channel_stats *stats = &s_stats[ch];
    if (stats->frames == 0 || stats->sum_sq == 0.0 || stats->tone_ref == 0.0) {
        return 0.0;
    }
    return stats->tone * (double)stats->frames / (stats->tone_ref * stats->sum_sq);
}

static void analysis_reset(size_t channels)
{
    s_analysis_frames = 0;
    s_analysis_channels = channels;
}

static void analysis_store(const int16_t *frames, size_t frame_count, size_t channels)
{
    if (channels != s_analysis_channels || s_analysis_frames >= ANALYSIS_FRAMES) {
        return;
    }
    size_t copy = ANALYSIS_FRAMES - s_analysis_frames;
    if (copy > frame_count) {
        copy = frame_count;
    }
    memcpy(&s_analysis[s_analysis_frames * channels], frames, copy * channels * sizeof(int16_t));
    s_analysis_frames += copy;
}

// 扫描用 float: ESP32-S3 只有单精度硬件浮点, double 走软件模拟, 扫一遍要 1.5 s.
static double goertzel_power(size_t ch, uint32_t sample_rate, double freq)
{
    const float coeff = 2.0f * cosf(2.0f * (float)M_PI * (float)freq / (float)sample_rate);
    float s1 = 0.0f;
    float s2 = 0.0f;
    for (size_t i = 0; i < s_analysis_frames; ++i) {
        const float s0 = (float)s_analysis[i * s_analysis_channels + ch] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (double)(s1 * s1 + s2 * s2 - coeff * s1 * s2);
}

// 诊断: 扫描 100..4000 Hz 找主频, 并打印 12 个原始采样. 判据不依赖这些输出.
static void log_analysis(size_t ch, uint32_t sample_rate)
{
    if (s_analysis_frames == 0 || ch >= s_analysis_channels) {
        return;
    }
    const double mean_sq = s_stats[ch].frames == 0 ? 0.0 : s_stats[ch].sum_sq / (double)s_stats[ch].frames;
    const double norm = (double)s_analysis_frames * (double)s_analysis_frames * mean_sq;

    double best_ratio = 0.0;
    double best_freq = 0.0;
    for (double freq = SCAN_MIN_HZ; freq <= SCAN_MAX_HZ && norm > 0.0; freq += SCAN_STEP_HZ) {
        const double ratio = goertzel_power(ch, sample_rate, freq) / norm;
        if (ratio > best_ratio) {
            best_ratio = ratio;
            best_freq = freq;
        }
    }
    ESP_LOGI(TAG, "ch%u scan: peak=%.0f Hz ratio=%.3f (%.0f frames)", (unsigned)ch, best_freq, best_ratio,
             (double)s_analysis_frames);

    char line[192];
    size_t used = (size_t)snprintf(line, sizeof(line), "ch%u raw:", (unsigned)ch);
    for (size_t i = 0; i < 12 && i < s_analysis_frames && used < sizeof(line); ++i) {
        used += (size_t)snprintf(line + used, sizeof(line) - used, " %d",
                                (int)s_analysis[i * s_analysis_channels + ch]);
    }
    ESP_LOGI(TAG, "%s", line);
}

static void log_channels(size_t channels)
{
    for (size_t ch = 0; ch < channels; ++ch) {
        ESP_LOGI(TAG, "ch%u: rms=%.1f peak=%d tone_ratio=%.3f", (unsigned)ch, stats_rms(ch),
                 (int)s_stats[ch].peak, stats_tone_ratio(ch));
    }
}

static void play_tone(uint32_t sample_rate, uint32_t duration_ms)
{
    const size_t total_frames = (size_t)sample_rate * duration_ms / 1000;
    size_t sent = 0;
    while (sent < total_frames) {
        size_t frames = total_frames - sent;
        if (frames > CHUNK_FRAMES) {
            frames = CHUNK_FRAMES;
        }
        fill_tone(s_play_chunk, frames, sample_rate);

        size_t written = 0;
        TEST_ASSERT_EQUAL(ESP_OK,
                          bsp_audio_play_write(s_audio, s_play_chunk, frames * sizeof(s_play_chunk[0]), &written,
                                               IO_TIMEOUT_MS));
        TEST_ASSERT_EQUAL_UINT(frames * sizeof(s_play_chunk[0]), written);
        sent += frames;
    }
}

static void record_for(uint32_t sample_rate, size_t channels, uint32_t duration_ms)
{
    const size_t chunk_bytes = CHUNK_FRAMES * channels * sizeof(s_rec_chunk[0]);
    const size_t total_frames = (size_t)sample_rate * duration_ms / 1000;
    stats_reset(channels);

    size_t captured = 0;
    while (captured < total_frames) {
        size_t read = 0;
        TEST_ASSERT_EQUAL(ESP_OK,
                          bsp_audio_record_read(s_audio, s_rec_chunk, chunk_bytes, &read, IO_TIMEOUT_MS));
        TEST_ASSERT_EQUAL_UINT(0, read % (channels * sizeof(s_rec_chunk[0])));
        const size_t frames = read / (channels * sizeof(s_rec_chunk[0]));
        TEST_ASSERT_TRUE(frames > 0);
        stats_add(s_rec_chunk, frames, channels, sample_rate);
        captured += frames;
    }
}

// 同时播放和录音: 写一块读一块, 但两条流各自记进度.
// 录音一旦短读, played 会领先 captured; 播放写满 total_frames 后必须停止写,
// 否则会落到 play_write(len=0) 的参数错误上, 把测试判成假失败.
static void play_and_record(uint32_t sample_rate, size_t channels, uint32_t duration_ms)
{
    const size_t total_frames = (size_t)sample_rate * duration_ms / 1000;
    stats_reset(channels);

    size_t played = 0;
    size_t captured = 0;
    while (captured < total_frames) {
        if (played < total_frames) {
            size_t frames = total_frames - played;
            if (frames > CHUNK_FRAMES) {
                frames = CHUNK_FRAMES;
            }
            fill_tone(s_play_chunk, frames, sample_rate);

            size_t written = 0;
            TEST_ASSERT_EQUAL(ESP_OK,
                              bsp_audio_play_write(s_audio, s_play_chunk, frames * sizeof(s_play_chunk[0]), &written,
                                                   IO_TIMEOUT_MS));
            TEST_ASSERT_EQUAL_UINT(frames * sizeof(s_play_chunk[0]), written);
            played += frames;
        }

        size_t want = total_frames - captured;
        if (want > CHUNK_FRAMES) {
            want = CHUNK_FRAMES;
        }
        size_t read = 0;
        TEST_ASSERT_EQUAL(ESP_OK,
                          bsp_audio_record_read(s_audio, s_rec_chunk, want * channels * sizeof(s_rec_chunk[0]), &read,
                                                IO_TIMEOUT_MS));
        const size_t got = read / (channels * sizeof(s_rec_chunk[0]));
        TEST_ASSERT_TRUE(got > 0);
        analysis_store(s_rec_chunk, got, channels);
        stats_add(s_rec_chunk, got, channels, sample_rate);
        captured += got;
    }
}

// 人工项: y = 通过, n = 失败, 超时/无输入 = pending, 既不算通过也不算失败.
static void human_check(const char *item, const char *prompt)
{
    const int answer = selftest_human_check(MODULE, item, prompt, HUMAN_TIMEOUT_MS);
    if (answer < 0) {
        TEST_IGNORE_MESSAGE("no answer within the window: human item pending");
    }
    TEST_ASSERT_EQUAL_MESSAGE(1, answer, "operator reported that the audio output is wrong");
}

TEST_CASE("audio: desc is sane and open/close works", "[audio]")
{
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_TRUE(desc->present);
    TEST_ASSERT_TRUE(desc->has_playback);
    TEST_ASSERT_TRUE(desc->shared_clock);
    TEST_ASSERT_TRUE(desc->sample_rate_min <= desc->sample_rate_max);
    ESP_LOGI(TAG, "desc: record=%d loopback=%d full_duplex=%d rate=%u..%u", (int)desc->has_record,
             (int)desc->has_loopback, (int)desc->supports_full_duplex, (unsigned)desc->sample_rate_min,
             (unsigned)desc->sample_rate_max);

    const bsp_audio_config_t config = bsp_audio_default_config();
    TEST_ASSERT_EQUAL_UINT8(16, config.bits_per_sample);
    TEST_ASSERT_TRUE(config.sample_rate >= desc->sample_rate_min && config.sample_rate <= desc->sample_rate_max);
    TEST_ASSERT_EQUAL_UINT8(desc->has_record ? (BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_MIC2) : 0,
                            config.record_channel_mask);

    open_audio(&config);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_close(s_audio));
    s_audio = NULL;
}

TEST_CASE("audio: second open rejected until closed", "[audio]")
{
    const bsp_audio_config_t config = bsp_audio_default_config();
    open_audio(&config);

    bsp_audio_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_audio_open(&config, &second));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_close(s_audio));
    s_audio = NULL;

    open_audio(&config);
}

TEST_CASE("audio: invalid configs rejected", "[audio]")
{
    bsp_audio_config_t config = bsp_audio_default_config();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_open(NULL, &s_audio));

    // open 失败时必须把 handle_out 清空, 否则调用方会拿到一个未初始化的句柄.
    s_audio = (bsp_audio_handle_t)(uintptr_t)0x1;
    config.bits_per_sample = 8;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, bsp_audio_open(&config, &s_audio));
    TEST_ASSERT_NULL(s_audio);

    config = bsp_audio_default_config();
    config.sample_rate = 12345;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, bsp_audio_open(&config, &s_audio));
    config.sample_rate = 96000;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, bsp_audio_open(&config, &s_audio));

    config = bsp_audio_default_config();
    config.record_channel_mask = BSP_AUDIO_RECORD_CH_MIC2;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, bsp_audio_open(&config, &s_audio));
    config.record_channel_mask = BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_LOOPBACK;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, bsp_audio_open(&config, &s_audio));

    // 失败的 open 不能留下半开的会话.
    config = bsp_audio_default_config();
    open_audio(&config);
}

TEST_CASE("audio: record path is closed without a record mask", "[audio]")
{
    bsp_audio_config_t config = bsp_audio_default_config();
    config.record_channel_mask = 0;
    open_audio(&config);

    size_t transferred = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_audio_record_start(s_audio));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_audio_record_stop(s_audio));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      bsp_audio_record_set_gain(s_audio, BSP_AUDIO_RECORD_CH_MIC1, RECORD_GAIN_DB));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      bsp_audio_record_read(s_audio, s_rec_chunk, sizeof(s_rec_chunk), &transferred, IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT(0, transferred);

    // 只有播放的会话仍然可以正常播放.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_start(s_audio));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_stop(s_audio));
}

TEST_CASE("audio: null and out-of-range arguments rejected", "[audio]")
{
    size_t transferred = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_open(NULL, &s_audio));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_open(&(bsp_audio_config_t){ .sample_rate = 16000,
                                                                                .bits_per_sample = 16,
                                                                                .record_channel_mask = 0 }, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_close(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_play_start(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_play_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_play_set_volume(NULL, PLAY_VOLUME));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_play_set_mute(NULL, true));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_play_write(NULL, s_play_chunk, sizeof(s_play_chunk), &transferred, IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_record_start(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_record_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_record_set_gain(NULL, BSP_AUDIO_RECORD_CH_MIC1, RECORD_GAIN_DB));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_record_read(NULL, s_rec_chunk, sizeof(s_rec_chunk), &transferred, IO_TIMEOUT_MS));

    const bsp_audio_config_t config = bsp_audio_default_config();
    open_audio(&config);

    // 音量接受越界值并夹紧 (driver 侧夹到 0..100), 不是错误.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_set_volume(s_audio, -10));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_set_volume(s_audio, 150));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_set_volume(s_audio, PLAY_VOLUME));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_set_mute(s_audio, true));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_set_mute(s_audio, false));

    // play_write 只能在播放流启动后使用, 且长度按 16-bit 对齐.
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      bsp_audio_play_write(s_audio, s_play_chunk, sizeof(s_play_chunk), &transferred, IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_start(s_audio));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_play_write(s_audio, NULL, sizeof(s_play_chunk), &transferred, IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_play_write(s_audio, s_play_chunk, 0, &transferred, IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_play_write(s_audio, s_play_chunk, sizeof(s_play_chunk) - 1, &transferred,
                                           IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_play_write(s_audio, s_play_chunk, sizeof(s_play_chunk), NULL,
                                                                IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_stop(s_audio));

    // 录音增益只接受会话 mask 的子集.
    const size_t channels = record_channels(config.record_channel_mask);
    TEST_ASSERT_TRUE(channels > 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_audio_record_set_gain(s_audio, 0, RECORD_GAIN_DB));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_record_set_gain(s_audio, BSP_AUDIO_RECORD_CH_LOOPBACK, RECORD_GAIN_DB));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_set_gain(s_audio, config.record_channel_mask, RECORD_GAIN_DB));

    // record_read 的长度按帧对齐.
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      bsp_audio_record_read(s_audio, s_rec_chunk, sizeof(s_rec_chunk), &transferred, IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_start(s_audio));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_record_read(s_audio, s_rec_chunk, channels * sizeof(s_rec_chunk[0]) - 1, &transferred,
                                            IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_audio_record_read(s_audio, s_rec_chunk, sizeof(s_rec_chunk), NULL, IO_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_stop(s_audio));
}

TEST_CASE("audio: start and stop are idempotent", "[audio]")
{
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    const bsp_audio_config_t config = bsp_audio_default_config();
    open_audio(&config);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_start(s_audio));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_start(s_audio));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_stop(s_audio));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_stop(s_audio));

    if (desc->has_record) {
        TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_start(s_audio));
        TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_start(s_audio));
        TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_stop(s_audio));
        TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_stop(s_audio));
    }
}

TEST_CASE("audio: playback tone is audible", "[audio]")
{
    // 台位可能没接喇叭; 回采点接在 ES8311 输出 (PA 之前), 所以喇叭缺席不影响
    // 回采用例, 只影响这一条人工判据.
    const int speaker = selftest_human_check(MODULE, "speaker-attached",
                                             "is a speaker attached to the speaker connector?", HUMAN_TIMEOUT_MS);
    if (speaker < 0) {
        TEST_IGNORE_MESSAGE("no answer: speaker state unknown, audibility item pending");
    }
    if (speaker == 0) {
        TEST_IGNORE_MESSAGE("no speaker attached: tone audibility cannot be verified");
    }

    const bsp_audio_config_t config = bsp_audio_default_config();
    open_audio(&config);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_set_volume(s_audio, PLAY_VOLUME));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_start(s_audio));
    ESP_LOGI(TAG, ">>> playing a %d Hz tone for %d ms", TONE_HZ, TONE_PLAY_MS);
    play_tone(config.sample_rate, TONE_PLAY_MS);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_stop(s_audio));

    human_check("tone", "did the 1 kHz tone play clearly from the speaker?");
}

TEST_CASE("audio: loopback captures the playback tone", "[audio]")
{
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if (!desc->has_loopback || !desc->has_record) {
        TEST_IGNORE_MESSAGE("board has no loopback record path");
    }

    bsp_audio_config_t config = bsp_audio_default_config();
    config.record_channel_mask = BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_MIC2 | BSP_AUDIO_RECORD_CH_LOOPBACK;
    open_audio(&config);

    const size_t channels = record_channels(config.record_channel_mask);
    TEST_ASSERT_EQUAL_UINT(3, channels);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_set_gain(s_audio, BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_MIC2,
                                                        RECORD_GAIN_DB));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_set_gain(s_audio, BSP_AUDIO_RECORD_CH_LOOPBACK, LOOPBACK_GAIN_DB));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_set_volume(s_audio, PLAY_VOLUME));

    // 先录静音基线: 录音已启动但播放还没开始, 回采通道此时应该只有底噪.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_start(s_audio));
    analysis_reset(channels);
    record_for(config.sample_rate, channels, SILENCE_CAPTURE_MS);
    double silent_rms[MAX_RECORD_CHANNELS];
    double silent_peak[MAX_RECORD_CHANNELS];
    for (size_t ch = 0; ch < channels; ++ch) {
        silent_rms[ch] = stats_rms(ch);
        silent_peak[ch] = (double)s_stats[ch].peak;
    }
    ESP_LOGI(TAG, "silent baseline: ch0 rms=%.1f/peak=%.0f ch1 rms=%.1f/peak=%.0f ch2 rms=%.1f/peak=%.0f",
             silent_rms[0], silent_peak[0], silent_rms[1], silent_peak[1], silent_rms[2], silent_peak[2]);

    analysis_reset(channels);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_start(s_audio));
    ESP_LOGI(TAG, ">>> playing and recording at the same time for %d ms", TONE_CAPTURE_MS);
    play_and_record(config.sample_rate, channels, TONE_CAPTURE_MS);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_stop(s_audio));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_stop(s_audio));

    log_channels(channels);
    for (size_t ch = 0; ch < channels; ++ch) {
        log_analysis(ch, config.sample_rate);
    }

    // LOOPBACK 是第二个录制通道 (TDM slot 1): 播放的 1 kHz 必须出现在它上面,
    // 而且要比静音基线明显抬起来.
    const size_t loopback = LOOPBACK_CHANNEL;
    TEST_ASSERT_TRUE_MESSAGE(stats_rms(loopback) >= LOOPBACK_MIN_RMS, "loopback channel is silent");
    TEST_ASSERT_TRUE_MESSAGE(stats_rms(loopback) >= 3.0 * silent_rms[loopback],
                             "loopback signal does not come from playback");
    TEST_ASSERT_TRUE_MESSAGE(stats_tone_ratio(loopback) >= LOOPBACK_MIN_TONE_RATIO,
                             "loopback channel does not carry the 1 kHz tone");
}

TEST_CASE("audio: microphones pick up sound", "[audio]")
{
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if (!desc->has_record) {
        TEST_IGNORE_MESSAGE("board has no record path");
    }

    bsp_audio_config_t config = bsp_audio_default_config();
    config.record_channel_mask = BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_MIC2;
    open_audio(&config);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_set_gain(s_audio, config.record_channel_mask, RECORD_GAIN_DB));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_start(s_audio));
    ESP_LOGI(TAG, ">>> talk or tap near the two microphones for %d ms", MIC_CAPTURE_MS);
    record_for(config.sample_rate, 2, MIC_CAPTURE_MS);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_record_stop(s_audio));

    log_channels(2);
    TEST_ASSERT_TRUE_MESSAGE(stats_rms(0) >= MIC_MIN_RMS, "MIC1 picked up nothing");
    TEST_ASSERT_TRUE_MESSAGE(stats_rms(1) >= MIC_MIN_RMS, "MIC2 picked up nothing");
}

TEST_CASE("audio: non-default sample rates stream", "[audio]")
{
    const uint32_t rates[] = { 8000, 48000 };

    for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        bsp_audio_config_t config = bsp_audio_default_config();
        config.sample_rate = rates[i];
        open_audio(&config);

        TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_start(s_audio));
        play_tone(config.sample_rate, 100);
        TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_play_stop(s_audio));

        TEST_ASSERT_EQUAL(ESP_OK, bsp_audio_close(s_audio));
        s_audio = NULL;
    }
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no audio on this board");
        return;
    }

    selftest_run(MODULE);
}

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_audio.h"
#include "bsp_backlight.h"
#include "bsp_camera.h"
#include "bsp_display.h"
#include "bsp_board.h"
#include "bsp_gnss.h"
#include "bsp_imu.h"
#include "bsp_pmu.h"
#include "bsp_sdcard.h"
#include "bsp_touch.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_vfs_fat.h"
#include "shell.h"
#include "shell_internal.h"

#define TAG "shell_bsp"
#define GNSS_READ_BUF_SIZE 256
#define GNSS_DEFAULT_TIMEOUT_MS 1000
#define SD_DEFAULT_PATH "/sdcard"
#define AUDIO_FRAME_SAMPLES 256
#define AUDIO_CHANNELS 2
#define AUDIO_TONE_DEFAULT_VOLUME 60
#define AUDIO_TONE_AMPLITUDE 8000
#define AUDIO_REC_DEFAULT_GAIN_DB 30.0f
#define AUDIO_REC_DEFAULT_MS 1000
#define AUDIO_REC_MAX_MS 10000
#define AUDIO_TONE_MAX_MS 10000
#define AUDIO_TONE_MIN_HZ 20
#define AUDIO_TONE_MAX_HZ 8000

static bsp_backlight_handle_t s_backlight;

static const int16_t s_sine_table[] = {
    0, 6393, 12539, 18204, 23170, 27245, 30273, 32137,
    32767, 32137, 30273, 27245, 23170, 18204, 12539, 6393,
    0, -6393, -12539, -18204, -23170, -27245, -30273, -32137,
    -32767, -32137, -30273, -27245, -23170, -18204, -12539, -6393,
};

struct audio_stats {
    uint64_t sum_sq[AUDIO_CHANNELS];
    int16_t min[AUDIO_CHANNELS];
    int16_t max[AUDIO_CHANNELS];
    int32_t peak[AUDIO_CHANNELS];
    uint32_t count[AUDIO_CHANNELS];
};

static const char *yes_no(bool value)
{
    return value ? "yes" : "no";
}

static bool parse_u32(const char *text, uint32_t min_value, uint32_t max_value, uint32_t *out_value)
{
    if (text == NULL || out_value == NULL) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value < min_value || value > max_value) {
        return false;
    }

    *out_value = (uint32_t)value;
    return true;
}

static int print_ret(const char *op, esp_err_t ret)
{
    if (ret == ESP_OK) {
        return 0;
    }
    printf("%s failed: %s\n", op, esp_err_to_name(ret));
    return 1;
}

static uint64_t isqrt_u64(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = 1ULL << 62;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

static const char *sd_type_name(bsp_sdcard_type_t type)
{
    switch (type) {
    case BSP_SDCARD_TYPE_SDSC:
        return "SDSC";
    case BSP_SDCARD_TYPE_SDHC:
        return "SDHC";
    case BSP_SDCARD_TYPE_SDXC:
        return "SDXC";
    case BSP_SDCARD_TYPE_MMC:
        return "MMC";
    case BSP_SDCARD_TYPE_SDIO:
        return "SDIO";
    default:
        return "unknown";
    }
}

static const char *pmu_charge_state_name(bsp_pmu_charge_state_t state)
{
    switch (state) {
    case BSP_PMU_CHARGE_STATE_TRICKLE:
        return "trickle";
    case BSP_PMU_CHARGE_STATE_PRECHARGE:
        return "precharge";
    case BSP_PMU_CHARGE_STATE_CONSTANT_CURRENT:
        return "constant-current";
    case BSP_PMU_CHARGE_STATE_CONSTANT_VOLTAGE:
        return "constant-voltage";
    case BSP_PMU_CHARGE_STATE_DONE:
        return "done";
    case BSP_PMU_CHARGE_STATE_NOT_CHARGING:
        return "not-charging";
    default:
        return "unknown";
    }
}

static void print_pmu_events(bsp_pmu_event_t events)
{
    if (events == BSP_PMU_EVENT_NONE) {
        printf("events: none\n");
        return;
    }
    printf("events:");
    if (events & BSP_PMU_EVENT_VBUS_INSERT) {
        printf(" vbus-insert");
    }
    if (events & BSP_PMU_EVENT_VBUS_REMOVE) {
        printf(" vbus-remove");
    }
    if (events & BSP_PMU_EVENT_BATTERY_INSERT) {
        printf(" battery-insert");
    }
    if (events & BSP_PMU_EVENT_BATTERY_REMOVE) {
        printf(" battery-remove");
    }
    if (events & BSP_PMU_EVENT_CHARGE_START) {
        printf(" charge-start");
    }
    if (events & BSP_PMU_EVENT_CHARGE_DONE) {
        printf(" charge-done");
    }
    if (events & BSP_PMU_EVENT_POWER_KEY_SHORT) {
        printf(" power-key-short");
    }
    if (events & BSP_PMU_EVENT_POWER_KEY_LONG) {
        printf(" power-key-long");
    }
    printf("\n");
}

static const char *default_sd_path(void)
{
    if (shell_mount_get_count_internal() > 0) {
        const char *mount_path = shell_mount_get_path_internal(0);
        if (mount_path != NULL && mount_path[0] != '\0') {
            return mount_path;
        }
    }
    return SD_DEFAULT_PATH;
}

static int print_statvfs_info(const char *path)
{
    uint64_t total = 0;
    uint64_t free_bytes = 0;
    esp_err_t ret = esp_vfs_fat_info(path, &total, &free_bytes);
    if (ret != ESP_OK) {
        printf("sd: %s: %s\n", path, esp_err_to_name(ret));
        return 1;
    }

    printf("path: %s\n", path);
    printf("bytes: total=%" PRIu64 " free=%" PRIu64 " used=%" PRIu64 "\n",
           total, free_bytes, total - free_bytes);
    return 0;
}

static int cmd_bsp(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "info") != 0 && strcmp(argv[1], "desc") != 0)) {
        printf("usage: bsp info|desc\n");
        return 1;
    }

    const bsp_board_info_t *board = bsp_board_get_info();
    const bsp_touch_desc_t *touch = bsp_touch_get_desc();
    const bsp_backlight_desc_t *backlight = bsp_backlight_get_desc();
    const bsp_imu_desc_t *imu = bsp_imu_get_desc();
    const bsp_gnss_desc_t *gnss = bsp_gnss_get_desc();
    const bsp_sdcard_desc_t *sdcard = bsp_sdcard_get_desc();
    const bsp_camera_desc_t *camera = bsp_camera_get_desc();
    const bsp_audio_desc_t *audio = bsp_audio_get_desc();
    const bsp_pmu_desc_t *pmu = bsp_pmu_get_desc();

    printf("display: present=%s\n", yes_no(board != NULL && bsp_display_get_info(NULL)->present));

    printf("touch: present=%s", yes_no(touch != NULL && touch->present));
    if (touch != NULL && touch->present) {
        printf(" max_points=%u range=%ux%u", (unsigned)touch->max_points,
               (unsigned)touch->x_max, (unsigned)touch->y_max);
    }
    printf("\n");

    printf("backlight: present=%s\n", yes_no(backlight != NULL && backlight->present));
    printf("imu: present=%s\n", yes_no(imu != NULL && imu->present));
    printf("gnss: present=%s\n", yes_no(gnss != NULL && gnss->present));
    printf("sdcard: present=%s\n", yes_no(sdcard != NULL && sdcard->present));
    printf("audio: present=%s", yes_no(audio != NULL && audio->present));
    if (audio != NULL && audio->present) {
        printf(" playback=%s record=%s full_duplex=%s shared_clock=%s",
               yes_no(audio->has_playback), yes_no(audio->has_record),
               yes_no(audio->supports_full_duplex), yes_no(audio->shared_clock));
    }
    printf("\n");

    printf("camera: present=%s", yes_no(camera != NULL && camera->present));
    if (camera != NULL && camera->present) {
        printf(" size=%ux%u format=%d", (unsigned)camera->width, (unsigned)camera->height,
               (int)camera->pixel_format);
    }
    printf("\n");

    printf("pmu: present=%s", yes_no(pmu != NULL && pmu->present));
    if (pmu != NULL && pmu->present && pmu->model != NULL) {
        printf(" model=%s", pmu->model);
    }
    printf("\n");
    return 0;
}

static int cmd_pmu(int argc, char **argv)
{
    if (argc < 2 || argc > 3 ||
        (strcmp(argv[1], "status") != 0 && strcmp(argv[1], "events") != 0)) {
        printf("usage: pmu status|events [clear]\n");
        return 1;
    }

    const bsp_pmu_desc_t *desc = bsp_pmu_get_desc();
    if (desc == NULL || !desc->present) {
        printf("pmu: not present\n");
        return 1;
    }

    bool clear = false;
    if (argc == 3) {
        if (strcmp(argv[2], "clear") != 0) {
            printf("usage: pmu status|events [clear]\n");
            return 1;
        }
        clear = true;
    }

    bsp_pmu_handle_t pmu = NULL;
    const bsp_pmu_config_t cfg = BSP_PMU_CONFIG_DEFAULT();
    esp_err_t ret = bsp_pmu_open(&cfg, &pmu);
    if (ret != ESP_OK) {
        return print_ret("pmu open", ret);
    }

    if (strcmp(argv[1], "status") == 0) {
        bsp_pmu_status_t status = {0};
        ret = bsp_pmu_get_status(pmu, &status);
        esp_err_t close_ret = bsp_pmu_close(pmu);
        if (ret != ESP_OK) {
            return print_ret("pmu status", ret);
        }
        if (close_ret != ESP_OK) {
            return print_ret("pmu close", close_ret);
        }
        printf("vbus: present=%s good=%s voltage_mv=%d\n",
               yes_no(status.vbus_present), yes_no(status.vbus_good), status.vbus_voltage_mv);
        printf("battery: present=%s charging=%s discharging=%s percent=%d voltage_mv=%d\n",
               yes_no(status.battery_present), yes_no(status.charging), yes_no(status.discharging),
               status.battery_percent, status.battery_voltage_mv);
        printf("charge_state: %s\n", pmu_charge_state_name(status.charge_state));
        printf("system_voltage_mv: %d\n", status.system_voltage_mv);
        printf("pmu_temperature_c: %.2f\n", status.pmu_temperature_c);
        return 0;
    }

    bsp_pmu_event_t events = BSP_PMU_EVENT_NONE;
    ret = bsp_pmu_get_events(pmu, &events, clear);
    esp_err_t close_ret = bsp_pmu_close(pmu);
    if (ret != ESP_OK) {
        return print_ret("pmu events", ret);
    }
    if (close_ret != ESP_OK) {
        return print_ret("pmu close", close_ret);
    }
    print_pmu_events(events);
    return 0;
}

static int cmd_imu(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "read") != 0) {
        printf("usage: imu read\n");
        return 1;
    }

    const bsp_imu_desc_t *desc = bsp_imu_get_desc();
    if (desc == NULL || !desc->present) {
        printf("imu: not present\n");
        return 1;
    }

    bsp_imu_handle_t imu = NULL;
    esp_err_t ret = bsp_imu_open(&imu);
    if (ret != ESP_OK) {
        return print_ret("imu open", ret);
    }

    bsp_imu_data_t data = {0};
    ret = bsp_imu_read(imu, &data);
    esp_err_t close_ret = bsp_imu_close(imu);
    if (ret != ESP_OK) {
        return print_ret("imu read", ret);
    }
    if (close_ret != ESP_OK) {
        return print_ret("imu close", close_ret);
    }

    printf("accel_mps2: %.3f %.3f %.3f\n", data.accel_mps2_x, data.accel_mps2_y, data.accel_mps2_z);
    printf("gyro_rads: %.3f %.3f %.3f\n", data.gyro_rads_x, data.gyro_rads_y, data.gyro_rads_z);
    printf("temperature_c: %.2f\n", data.temperature_c);
    printf("timestamp_ms: %" PRIu32 "\n", data.timestamp_ms);
    return 0;
}

static int cmd_touch(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "read") != 0) {
        printf("usage: touch read\n");
        return 1;
    }

    const bsp_touch_desc_t *desc = bsp_touch_get_desc();
    if (desc == NULL || !desc->present) {
        printf("touch: not present\n");
        return 1;
    }

    bsp_touch_handle_t touch = NULL;
    esp_err_t ret = bsp_touch_open(&touch);
    if (ret != ESP_OK) {
        return print_ret("touch open", ret);
    }

    bsp_touch_point_t points[5] = {0};
    size_t points_num = 0;
    ret = bsp_touch_read(touch, points, sizeof(points) / sizeof(points[0]), &points_num);
    esp_err_t close_ret = bsp_touch_close(touch);
    if (ret != ESP_OK) {
        return print_ret("touch read", ret);
    }
    if (close_ret != ESP_OK) {
        return print_ret("touch close", close_ret);
    }

    printf("points: %u\n", (unsigned)points_num);
    for (size_t i = 0; i < points_num; i++) {
        printf("[%u] id=%u x=%u y=%u pressure=%u\n", (unsigned)i, (unsigned)points[i].id,
               (unsigned)points[i].x, (unsigned)points[i].y, (unsigned)points[i].pressure);
    }
    return 0;
}

static int cmd_gnss(int argc, char **argv)
{
    if (argc < 2 || argc > 3 || strcmp(argv[1], "read") != 0) {
        printf("usage: gnss read [timeout_ms]\n");
        return 1;
    }

    uint32_t timeout_ms = GNSS_DEFAULT_TIMEOUT_MS;
    if (argc == 3 && !parse_u32(argv[2], 0, 60000, &timeout_ms)) {
        printf("gnss: invalid timeout_ms\n");
        return 1;
    }

    const bsp_gnss_desc_t *desc = bsp_gnss_get_desc();
    if (desc == NULL || !desc->present) {
        printf("gnss: not present\n");
        return 1;
    }

    bsp_gnss_handle_t gnss = NULL;
    esp_err_t ret = bsp_gnss_open(&gnss);
    if (ret != ESP_OK) {
        return print_ret("gnss open", ret);
    }

    uint8_t buf[GNSS_READ_BUF_SIZE + 1] = {0};
    size_t read_len = 0;
    ret = bsp_gnss_read(gnss, buf, GNSS_READ_BUF_SIZE, &read_len, timeout_ms);
    esp_err_t close_ret = bsp_gnss_close(gnss);
    if (ret != ESP_OK) {
        return print_ret("gnss read", ret);
    }
    if (close_ret != ESP_OK) {
        return print_ret("gnss close", close_ret);
    }

    buf[read_len] = '\0';
    printf("read_len: %u\n", (unsigned)read_len);
    if (read_len > 0) {
        printf("%s\n", (const char *)buf);
    }
    return 0;
}

static int cmd_backlight(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "set") != 0) {
        printf("usage: backlight set <percent>\n");
        return 1;
    }

    const bsp_backlight_desc_t *desc = bsp_backlight_get_desc();
    if (desc == NULL || !desc->present) {
        printf("backlight: not present\n");
        return 1;
    }

    esp_err_t ret = ESP_OK;
    if (s_backlight == NULL) {
        ret = bsp_backlight_open(&s_backlight);
        if (ret != ESP_OK) {
            return print_ret("backlight open", ret);
        }
    }

    uint32_t percent = 0;
    if (!parse_u32(argv[2], 0, 100, &percent)) {
        printf("backlight: invalid percent\n");
        return 1;
    }
    ret = bsp_backlight_set_percent(s_backlight, (uint8_t)percent);
    if (ret == ESP_OK) {
        printf("backlight: %u%%\n", (unsigned)percent);
    }

    if (ret != ESP_OK) {
        return print_ret("backlight", ret);
    }
    return 0;
}

static int cmd_sd(int argc, char **argv)
{
    if (argc < 2 || argc > 3 || (strcmp(argv[1], "info") != 0 && strcmp(argv[1], "fs") != 0)) {
        printf("usage: sd info|fs [mount_path]\n");
        return 1;
    }

    const bsp_sdcard_desc_t *desc = bsp_sdcard_get_desc();
    if (desc == NULL || !desc->present) {
        printf("sd: not present\n");
        return 1;
    }

    const char *path = (argc == 3) ? argv[2] : default_sd_path();
    printf("present: yes\n");
    if (strcmp(argv[1], "info") == 0) {
        printf("type: %s\n", sd_type_name(BSP_SDCARD_TYPE_UNKNOWN));
        printf("note: card identity requires the owner mount handle; showing mounted FS stats instead\n");
    }
    return print_statvfs_info(path);
}

static void audio_fill_tone(int16_t *samples, size_t frames, uint32_t sample_rate, uint32_t freq_hz, uint32_t *phase)
{
    uint32_t phase_step = (freq_hz * (uint32_t)(sizeof(s_sine_table) / sizeof(s_sine_table[0])) * 65536u) / sample_rate;
    for (size_t i = 0; i < frames; i++) {
        uint32_t index = (*phase >> 16) % (sizeof(s_sine_table) / sizeof(s_sine_table[0]));
        int16_t value = (int16_t)((int32_t)s_sine_table[index] * AUDIO_TONE_AMPLITUDE / INT16_MAX);
        *phase += phase_step;
        for (uint8_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
            samples[i * AUDIO_CHANNELS + ch] = value;
        }
    }
}

static void audio_stats_init(struct audio_stats *stats)
{
    memset(stats, 0, sizeof(*stats));
    for (uint8_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        stats->min[ch] = INT16_MAX;
        stats->max[ch] = INT16_MIN;
    }
}

static void audio_stats_add(struct audio_stats *stats, const int16_t *samples, size_t frames)
{
    for (size_t i = 0; i < frames; i++) {
        for (uint8_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
            int16_t sample = samples[i * AUDIO_CHANNELS + ch];
            int32_t abs_sample = sample < 0 ? -(int32_t)sample : sample;
            if (sample < stats->min[ch]) {
                stats->min[ch] = sample;
            }
            if (sample > stats->max[ch]) {
                stats->max[ch] = sample;
            }
            if (abs_sample > stats->peak[ch]) {
                stats->peak[ch] = abs_sample;
            }
            stats->sum_sq[ch] += (uint64_t)((int64_t)sample * (int64_t)sample);
            stats->count[ch]++;
        }
    }
}

static int audio_tone(uint32_t freq_hz, uint32_t ms)
{
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if (desc == NULL || !desc->present || !desc->has_playback) {
        printf("audio: playback not present\n");
        return 1;
    }

    bsp_audio_config_t cfg = bsp_audio_default_config();
    bsp_audio_handle_t audio = NULL;
    esp_err_t ret = bsp_audio_open(&cfg, &audio);
    if (ret != ESP_OK) {
        return print_ret("audio open", ret);
    }

    int16_t *buf = calloc(AUDIO_FRAME_SAMPLES * AUDIO_CHANNELS, sizeof(int16_t));
    if (buf == NULL) {
        (void)bsp_audio_close(audio);
        return print_ret("audio alloc", ESP_ERR_NO_MEM);
    }

    ret = bsp_audio_play_start(audio);
    if (ret == ESP_OK) {
        ret = bsp_audio_play_set_volume(audio, AUDIO_TONE_DEFAULT_VOLUME);
    }

    uint32_t phase = 0;
    uint32_t frames_total = cfg.sample_rate * ms / 1000;
    for (uint32_t done = 0; ret == ESP_OK && done < frames_total; done += AUDIO_FRAME_SAMPLES) {
        size_t frames = frames_total - done;
        if (frames > AUDIO_FRAME_SAMPLES) {
            frames = AUDIO_FRAME_SAMPLES;
        }
        audio_fill_tone(buf, frames, cfg.sample_rate, freq_hz, &phase);
        size_t bytes = frames * AUDIO_CHANNELS * sizeof(int16_t);
        size_t written = 0;
        ret = bsp_audio_play_write(audio, buf, bytes, &written, UINT32_MAX);
        if (ret == ESP_OK && written != bytes) {
            ret = ESP_ERR_TIMEOUT;
        }
    }

    esp_err_t stop_ret = bsp_audio_play_stop(audio);
    free(buf);
    esp_err_t close_ret = bsp_audio_close(audio);
    if (ret != ESP_OK) {
        return print_ret("audio tone", ret);
    }
    if (stop_ret != ESP_OK) {
        return print_ret("audio stop", stop_ret);
    }
    if (close_ret != ESP_OK) {
        return print_ret("audio close", close_ret);
    }
    printf("audio tone: freq=%" PRIu32 "Hz ms=%" PRIu32 " volume=%u\n",
           freq_hz, ms, AUDIO_TONE_DEFAULT_VOLUME);
    return 0;
}

static int audio_rec_rms(uint32_t ms)
{
    const bsp_audio_desc_t *desc = bsp_audio_get_desc();
    if (desc == NULL || !desc->present || !desc->has_record) {
        printf("audio: record not present\n");
        return 1;
    }

    bsp_audio_config_t cfg = bsp_audio_default_config();
    bsp_audio_handle_t audio = NULL;
    esp_err_t ret = bsp_audio_open(&cfg, &audio);
    if (ret != ESP_OK) {
        return print_ret("audio open", ret);
    }

    int16_t *buf = calloc(AUDIO_FRAME_SAMPLES * AUDIO_CHANNELS, sizeof(int16_t));
    if (buf == NULL) {
        (void)bsp_audio_close(audio);
        return print_ret("audio alloc", ESP_ERR_NO_MEM);
    }

    struct audio_stats stats;
    audio_stats_init(&stats);

    ret = bsp_audio_record_start(audio);
    if (ret == ESP_OK) {
        ret = bsp_audio_record_set_gain(audio, AUDIO_REC_DEFAULT_GAIN_DB);
    }

    uint32_t frames_total = cfg.sample_rate * ms / 1000;
    for (uint32_t done = 0; ret == ESP_OK && done < frames_total; done += AUDIO_FRAME_SAMPLES) {
        size_t bytes = AUDIO_FRAME_SAMPLES * AUDIO_CHANNELS * sizeof(int16_t);
        size_t bytes_read = 0;
        ret = bsp_audio_record_read(audio, buf, bytes, &bytes_read, UINT32_MAX);
        if (ret == ESP_OK && (bytes_read % (AUDIO_CHANNELS * sizeof(int16_t))) != 0) {
            ret = ESP_FAIL;
        }
        if (ret == ESP_OK) {
            audio_stats_add(&stats, buf, bytes_read / (AUDIO_CHANNELS * sizeof(int16_t)));
        }
    }

    esp_err_t stop_ret = bsp_audio_record_stop(audio);
    free(buf);
    esp_err_t close_ret = bsp_audio_close(audio);
    if (ret != ESP_OK) {
        return print_ret("audio rec-rms", ret);
    }
    if (stop_ret != ESP_OK) {
        return print_ret("audio record stop", stop_ret);
    }
    if (close_ret != ESP_OK) {
        return print_ret("audio close", close_ret);
    }

    for (uint8_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        uint64_t rms = stats.count[ch] == 0 ? 0 : isqrt_u64(stats.sum_sq[ch] / stats.count[ch]);
        printf("ch%u: rms=%" PRIu64 " peak=%" PRIi32 " min=%d max=%d span=%d samples=%" PRIu32 "\n",
               (unsigned)ch, rms, stats.peak[ch], (int)stats.min[ch], (int)stats.max[ch],
               (int)stats.max[ch] - (int)stats.min[ch], stats.count[ch]);
    }
    return 0;
}

static int cmd_audio(int argc, char **argv)
{
    if (argc < 2 || argc > 4 || (strcmp(argv[1], "tone") != 0 && strcmp(argv[1], "rec-rms") != 0)) {
        printf("usage: audio tone <freq_hz> <ms>|rec-rms [ms]\n");
        return 1;
    }

    if (strcmp(argv[1], "tone") == 0) {
        uint32_t freq_hz = 0;
        uint32_t ms = 0;
        if (argc != 4 || !parse_u32(argv[2], AUDIO_TONE_MIN_HZ, AUDIO_TONE_MAX_HZ, &freq_hz) ||
            !parse_u32(argv[3], 1, AUDIO_TONE_MAX_MS, &ms)) {
            printf("usage: audio tone <freq_hz:%u..%u> <ms:1..%u>\n",
                   AUDIO_TONE_MIN_HZ, AUDIO_TONE_MAX_HZ, AUDIO_TONE_MAX_MS);
            return 1;
        }
        return audio_tone(freq_hz, ms);
    }

    uint32_t ms = AUDIO_REC_DEFAULT_MS;
    if (argc == 3 && !parse_u32(argv[2], 1, AUDIO_REC_MAX_MS, &ms)) {
        printf("usage: audio rec-rms [ms:1..%u]\n", AUDIO_REC_MAX_MS);
        return 1;
    }
    if (argc > 3) {
        printf("usage: audio rec-rms [ms]\n");
        return 1;
    }
    return audio_rec_rms(ms);
}

esp_err_t shell_register_bsp_commands(void)
{
    const esp_console_cmd_t bsp_cmd = {
        .command = "bsp",
        .help = "print BSP capability information",
        .hint = NULL,
        .func = &cmd_bsp,
    };
    const esp_console_cmd_t imu_cmd = {
        .command = "imu",
        .help = "BSP IMU debug commands",
        .hint = NULL,
        .func = &cmd_imu,
    };
    const esp_console_cmd_t touch_cmd = {
        .command = "touch",
        .help = "BSP touch debug commands",
        .hint = NULL,
        .func = &cmd_touch,
    };
    const esp_console_cmd_t gnss_cmd = {
        .command = "gnss",
        .help = "BSP GNSS debug commands",
        .hint = NULL,
        .func = &cmd_gnss,
    };
    const esp_console_cmd_t backlight_cmd = {
        .command = "backlight",
        .help = "BSP backlight debug commands",
        .hint = NULL,
        .func = &cmd_backlight,
    };
    const esp_console_cmd_t sd_cmd = {
        .command = "sd",
        .help = "BSP SD card debug commands",
        .hint = NULL,
        .func = &cmd_sd,
    };
    const esp_console_cmd_t audio_cmd = {
        .command = "audio",
        .help = "BSP audio debug commands",
        .hint = NULL,
        .func = &cmd_audio,
    };
    const esp_console_cmd_t pmu_cmd = {
        .command = "pmu",
        .help = "BSP PMU debug commands",
        .hint = NULL,
        .func = &cmd_pmu,
    };

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&bsp_cmd), TAG, "register bsp failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&imu_cmd), TAG, "register imu failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&touch_cmd), TAG, "register touch failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&gnss_cmd), TAG, "register gnss failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&backlight_cmd), TAG, "register backlight failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&sd_cmd), TAG, "register sd failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&audio_cmd), TAG, "register audio failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&pmu_cmd), TAG, "register pmu failed");
    return ESP_OK;
}

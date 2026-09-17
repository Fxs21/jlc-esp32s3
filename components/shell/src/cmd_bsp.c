#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_backlight.h"
#include "bsp_board.h"
#include "bsp_gnss.h"
#include "bsp_imu.h"
#include "bsp_sdcard.h"
#include "bsp_touch.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shell.h"
#include "shell_internal.h"

#define TAG "shell_bsp"
#define NMEA_LINE_MAX     160
#define GNSS_READ_BUF_SIZE 256
#define GNSS_DEFAULT_TIMEOUT_MS 1000
#define SD_DEFAULT_PATH "/sdcard"
static bsp_backlight_handle_t s_backlight;
static bsp_sdcard_handle_t s_sd;


static const char *yes_no(bool value)
{
    return value ? "yes" : "no";
}

static bool parse_u32(const char *text, uint32_t min_value, uint32_t max_value, uint32_t *value_out)
{
    if (text == NULL || value_out == NULL) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value < min_value || value > max_value) {
        return false;
    }

    *value_out = (uint32_t)value;
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

    if (board != NULL) {
        printf("board: %s (id=%d)\n", board->name, (int)board->id);
    }
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
    return 0;
}

static int cmd_imu(int argc, char **argv)
{
    if (argc < 2 || argc > 3 || strcmp(argv[1], "read") != 0) {
        printf("usage: imu read [count]\n");
        return 1;
    }

    uint32_t count = 1;
    if (argc == 3 && !parse_u32(argv[2], 1, 1000, &count)) {
        printf("imu: invalid count\n");
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

    uint32_t samples = 0;
    for (uint32_t i = 0; i < count; i++) {
        bool ready = false;
        ret = bsp_imu_is_data_ready(imu, &ready);
        if (ret != ESP_OK) {
            printf("imu data_ready failed: %s\n", esp_err_to_name(ret));
            break;
        }
        if (!ready) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        bsp_imu_data_t data = {0};
        ret = bsp_imu_read(imu, &data);
        if (ret != ESP_OK) {
            printf("imu read failed: %s\n", esp_err_to_name(ret));
            break;
        }
        samples++;

        printf("accel_mps2: %.3f %.3f %.3f  gyro_rads: %.3f %.3f %.3f  temp_c: %.2f  ts_ticks: %" PRIu32 "\n",
               data.accel_mps2_x, data.accel_mps2_y, data.accel_mps2_z,
               data.gyro_rads_x, data.gyro_rads_y, data.gyro_rads_z,
               data.temperature_c, data.timestamp_ticks);

        if (count > 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ret = bsp_imu_close(imu);
    if (ret != ESP_OK) {
        return print_ret("imu close", ret);
    }

    if (count > 1 && samples < count) {
        printf("imu: only got %" PRIu32 "/%" PRIu32 " samples\n", samples, count);
    }
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

/* ---------------------------------------------------------------------------
 * NMEA sentence helpers
 * -------------------------------------------------------------------------*/

typedef struct {
    char line[NMEA_LINE_MAX];
    size_t len;
    bool saw_rmc;
    bool saw_gga;
    bool saw_fix;
} nmea_state_t;

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

static bool nmea_checksum_ok(const char *line)
{
    if (line == NULL || line[0] != '$') {
        return false;
    }

    const char *star = strchr(line, '*');
    if (star == NULL || hex_value(star[1]) < 0 || hex_value(star[2]) < 0) {
        return false;
    }

    uint8_t checksum = 0;
    for (const char *p = line + 1; p < star; ++p) {
        checksum ^= (uint8_t)*p;
    }

    uint8_t expected = (uint8_t)((hex_value(star[1]) << 4) | hex_value(star[2]));
    return checksum == expected;
}

static bool nmea_is_type(const char *line, const char *type)
{
    return line != NULL && type != NULL && line[0] == '$'
           && strlen(line) >= 6 && strncmp(line + 3, type, 3) == 0;
}

static bool nmea_get_field(const char *line, int field_index,
                           char *field_out, size_t size_out)
{
    if (line == NULL || field_out == NULL || size_out == 0
        || field_index < 0 || line[0] != '$') {
        return false;
    }

    int current = 0;
    const char *start = line + 1;
    for (const char *p = line + 1;; ++p) {
        if (*p == ',' || *p == '*' || *p == '\0') {
            if (current == field_index) {
                size_t len = (size_t)(p - start);
                if (len >= size_out) {
                    len = size_out - 1;
                }
                memcpy(field_out, start, len);
                field_out[len] = '\0';
                return true;
            }
            if (*p == '*' || *p == '\0') {
                break;
            }
            current++;
            start = p + 1;
        }
    }
    return false;
}

static bool nmea_parse_lat_lon(const char *value, const char *hemisphere,
                               double *deg_out)
{
    if (value == NULL || hemisphere == NULL || deg_out == NULL
        || value[0] == '\0' || hemisphere[0] == '\0') {
        return false;
    }

    double raw = strtod(value, NULL);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (double)degrees * 100.0;
    double result = (double)degrees + minutes / 60.0;
    if (hemisphere[0] == 'S' || hemisphere[0] == 'W') {
        result = -result;
    }

    *deg_out = result;
    return true;
}

static void nmea_print_rmc(const char *line)
{
    char utc[16] = {0};
    char status[4] = {0};
    char lat_raw[20] = {0};
    char lat_hemi[4] = {0};
    char lon_raw[20] = {0};
    char lon_hemi[4] = {0};
    char speed[16] = {0};
    char course[16] = {0};
    char date[16] = {0};

    (void)nmea_get_field(line, 1, utc, sizeof(utc));
    (void)nmea_get_field(line, 2, status, sizeof(status));
    (void)nmea_get_field(line, 3, lat_raw, sizeof(lat_raw));
    (void)nmea_get_field(line, 4, lat_hemi, sizeof(lat_hemi));
    (void)nmea_get_field(line, 5, lon_raw, sizeof(lon_raw));
    (void)nmea_get_field(line, 6, lon_hemi, sizeof(lon_hemi));
    (void)nmea_get_field(line, 7, speed, sizeof(speed));
    (void)nmea_get_field(line, 8, course, sizeof(course));
    (void)nmea_get_field(line, 9, date, sizeof(date));

    double lat = 0.0, lon = 0.0;
    bool has_pos = nmea_parse_lat_lon(lat_raw, lat_hemi, &lat)
                   && nmea_parse_lat_lon(lon_raw, lon_hemi, &lon);

    printf("  RMC utc=%s status=%s", utc, status);
    if (has_pos) {
        printf(" lat=%.6f lon=%.6f", lat, lon);
    }
    printf(" speed_kn=%s course=%s date=%s\n", speed, course, date);
}

static void nmea_print_gga(const char *line)
{
    char utc[16] = {0};
    char lat_raw[20] = {0};
    char lat_hemi[4] = {0};
    char lon_raw[20] = {0};
    char lon_hemi[4] = {0};
    char fix_quality[8] = {0};
    char satellites[8] = {0};
    char hdop[12] = {0};
    char altitude[16] = {0};

    (void)nmea_get_field(line, 1, utc, sizeof(utc));
    (void)nmea_get_field(line, 2, lat_raw, sizeof(lat_raw));
    (void)nmea_get_field(line, 3, lat_hemi, sizeof(lat_hemi));
    (void)nmea_get_field(line, 4, lon_raw, sizeof(lon_raw));
    (void)nmea_get_field(line, 5, lon_hemi, sizeof(lon_hemi));
    (void)nmea_get_field(line, 6, fix_quality, sizeof(fix_quality));
    (void)nmea_get_field(line, 7, satellites, sizeof(satellites));
    (void)nmea_get_field(line, 8, hdop, sizeof(hdop));
    (void)nmea_get_field(line, 9, altitude, sizeof(altitude));

    double lat = 0.0, lon = 0.0;
    bool has_pos = nmea_parse_lat_lon(lat_raw, lat_hemi, &lat)
                   && nmea_parse_lat_lon(lon_raw, lon_hemi, &lon);

    printf("  GGA utc=%s fix=%s sats=%s hdop=%s alt_m=%s",
           utc, fix_quality, satellites, hdop, altitude);
    if (has_pos) {
        printf(" lat=%.6f lon=%.6f", lat, lon);
    }
    printf("\n");
}

static void nmea_print_line(const char *line)
{
    if (!nmea_checksum_ok(line)) {
        printf("  (bad checksum: %s)\n", line);
        return;
    }

    if (nmea_is_type(line, "RMC")) {
        nmea_print_rmc(line);
    } else if (nmea_is_type(line, "GGA")) {
        nmea_print_gga(line);
    }
}

static void nmea_state_feed(nmea_state_t *s, const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        char ch = (char)buf[i];
        if (ch == '$') {
            s->len = 0;
        }

        if (ch == '\r' || ch == '\n') {
            if (s->len > 0) {
                s->line[s->len] = '\0';
                nmea_print_line(s->line);
                /* track fix status for summary */
                if (s->line[0] == '$' && strlen(s->line) >= 6) {
                    if (strncmp(s->line + 3, "RMC", 3) == 0) {
                        s->saw_rmc = true;
                    } else if (strncmp(s->line + 3, "GGA", 3) == 0) {
                        s->saw_gga = true;
                    }
                }
                s->len = 0;
            }
            continue;
        }

        if (s->len < sizeof(s->line) - 1) {
            s->line[s->len++] = ch;
        } else {
            s->len = 0;
        }
    }
}

/* ---------------------------------------------------------------------------
 * gnss command: read [count] [timeout_ms]
 * -------------------------------------------------------------------------*/

static int cmd_gnss(int argc, char **argv)
{
    if (argc < 2 || argc > 4 || strcmp(argv[1], "read") != 0) {
        printf("usage: gnss read [count] [timeout_ms]\n");
        return 1;
    }

    uint32_t count = 1;
    uint32_t timeout_ms = GNSS_DEFAULT_TIMEOUT_MS;

    int idx = 2;
    if (idx < argc && !parse_u32(argv[idx], 1, 100, &count)) {
        /* not a number, try as timeout */
        if (parse_u32(argv[idx], 0, 60000, &timeout_ms)) {
            count = 1;
        } else {
            printf("gnss: invalid count (1..100)\n");
            return 1;
        }
    }
    idx++;
    if (idx < argc && !parse_u32(argv[idx], 0, 60000, &timeout_ms)) {
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

    uint32_t reads = 0;
    size_t bytes_total = 0;
    nmea_state_t nmea = {0};
    uint8_t buf[GNSS_READ_BUF_SIZE] = {0};

    for (uint32_t i = 0; i < count; i++) {
        size_t read_len = 0;
        ret = bsp_gnss_read(gnss, buf, sizeof(buf), &read_len, timeout_ms);
        if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
            printf("gnss read failed: %s\n", esp_err_to_name(ret));
            break;
        }
        if (ret == ESP_ERR_TIMEOUT) {
            read_len = 0;
        }
        reads++;

        if (read_len == 0) {
            printf("[%u] no data\n", (unsigned)i);
        } else {
            printf("[%u] %u bytes\n", (unsigned)i, (unsigned)read_len);
            nmea_state_feed(&nmea, buf, read_len);
            bytes_total += read_len;
        }

        if (count > 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    esp_err_t close_ret = bsp_gnss_close(gnss);
    if (close_ret != ESP_OK) {
        return print_ret("gnss close", close_ret);
    }

    if (bytes_total > 0) {
        if (!nmea.saw_rmc && !nmea.saw_gga) {
            printf("gnss: no valid RMC/GGA sentences parsed (indoor?)\n");
        } else if (nmea.saw_rmc && nmea.saw_gga) {
            printf("gnss: RMC + GGA received\n");
        }
    }

    if (count > 1 && reads < count) {
        printf("gnss: only got %" PRIu32 "/%" PRIu32 " reads\n", reads, count);
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

// Open and mount the card once; the shell keeps the handle until "sd umount".
static esp_err_t sd_mount_once(const char *path)
{
    if (s_sd != NULL) {
        return ESP_OK;
    }

    esp_err_t ret = bsp_sdcard_open(&s_sd);
    if (ret != ESP_OK) {
        s_sd = NULL;
        return ret;
    }

    ret = bsp_sdcard_mount(s_sd, path);
    if (ret != ESP_OK) {
        (void)bsp_sdcard_close(s_sd);
        s_sd = NULL;
        return ret;
    }
    return ESP_OK;
}

static int cmd_sd(int argc, char **argv)
{
    if (argc < 2 || argc > 3 ||
        (strcmp(argv[1], "info") != 0 && strcmp(argv[1], "mount") != 0 &&
         strcmp(argv[1], "umount") != 0 && strcmp(argv[1], "fs") != 0)) {
        printf("usage: sd mount [mount_path] | umount | info | fs [mount_path]\n");
        return 1;
    }
    if (strcmp(argv[1], "info") == 0 && argc != 2) {
        printf("usage: sd info\n");
        return 1;
    }

    const bsp_sdcard_desc_t *desc = bsp_sdcard_get_desc();
    if (desc == NULL || !desc->present) {
        printf("sd: not present\n");
        return 1;
    }

    if (strcmp(argv[1], "umount") == 0) {
        if (s_sd == NULL) {
            printf("sd: not mounted by shell\n");
            return 1;
        }
        esp_err_t unmount_ret = bsp_sdcard_unmount(s_sd);
        esp_err_t close_ret = bsp_sdcard_close(s_sd);
        s_sd = NULL;
        if (unmount_ret != ESP_OK) {
            return print_ret("sd unmount", unmount_ret);
        }
        if (close_ret != ESP_OK) {
            return print_ret("sd close", close_ret);
        }
        printf("sd: unmounted\n");
        return 0;
    }

    if (strcmp(argv[1], "fs") == 0) {
        const char *path = (argc == 3) ? argv[2] : default_sd_path();
        return print_statvfs_info(path);
    }

    if (strcmp(argv[1], "info") == 0) {
        if (s_sd == NULL) {
            printf("sd: not mounted by shell; run \"sd mount\" first\n");
            return 1;
        }
        printf("present: yes\n");

        bsp_sdcard_info_t info = {0};
        esp_err_t ret = bsp_sdcard_get_info(s_sd, &info);
        if (ret == ESP_OK) {
            printf("card name      : %s\n", info.name);
            printf("card type      : %s\n", sd_type_name(info.type));
            printf("capacity bytes : %" PRIu64 "\n", info.capacity_bytes);
            printf("sector count   : %" PRIu64 "\n", info.sector_count);
            printf("sector size    : %" PRIu32 "\n", info.sector_size);
            printf("bus width      : %u\n", info.bus_width);
            printf("real freq kHz  : %d\n", info.real_freq_khz);
        } else {
            printf("get_info: %s\n", esp_err_to_name(ret));
        }

        bsp_sdcard_fs_info_t fs = {0};
        ret = bsp_sdcard_get_fs_info(s_sd, &fs);
        if (ret == ESP_OK) {
            printf("fs total bytes : %" PRIu64 "\n", fs.total_bytes);
            printf("fs free bytes  : %" PRIu64 "\n", fs.free_bytes);
        }
        return 0;
    }

    const char *path = (argc == 3) ? argv[2] : default_sd_path();

    if (s_sd != NULL) {
        const char *mount_point = NULL;
        if (bsp_sdcard_get_mount_point(s_sd, &mount_point) == ESP_OK) {
            printf("sd: already mounted at %s\n", mount_point);
        } else {
            printf("sd: already mounted\n");
        }
        return 0;
    }

    esp_err_t ret = sd_mount_once(path);
    if (ret == ESP_ERR_INVALID_STATE) {
        printf("card handle already open by app; use \"sd fs\" for filesystem stats\n");
        return 1;
    }
    if (ret != ESP_OK) {
        return print_ret("sd mount", ret);
    }
    const char *mount_point = NULL;
    if (bsp_sdcard_get_mount_point(s_sd, &mount_point) == ESP_OK) {
        printf("sd: mounted at %s\n", mount_point);
    } else {
        printf("sd: mounted\n");
    }
    return 0;
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

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&bsp_cmd), TAG, "register bsp failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&imu_cmd), TAG, "register imu failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&touch_cmd), TAG, "register touch failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&gnss_cmd), TAG, "register gnss failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&backlight_cmd), TAG, "register backlight failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&sd_cmd), TAG, "register sd failed");
    return ESP_OK;
}

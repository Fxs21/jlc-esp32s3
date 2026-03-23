#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_gnss.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "test_bsp_gnss"
#define READ_TIMEOUT_MS 1000
#define READ_COUNT      20
#define NMEA_LINE_MAX   160
#define PRINT_RAW_NMEA  0

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
    return line != NULL && type != NULL && line[0] == '$' && strlen(line) >= 6 && strncmp(line + 3, type, 3) == 0;
}

static bool nmea_get_field(const char *line, int field_index, char *out, size_t out_size)
{
    if (line == NULL || out == NULL || out_size == 0 || field_index < 0 || line[0] != '$') {
        return false;
    }

    int current = 0;
    const char *start = line + 1;
    for (const char *p = line + 1;; ++p) {
        if (*p == ',' || *p == '*' || *p == '\0') {
            if (current == field_index) {
                size_t len = (size_t)(p - start);
                if (len >= out_size) {
                    len = out_size - 1;
                }
                memcpy(out, start, len);
                out[len] = '\0';
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

static bool nmea_parse_lat_lon(const char *value, const char *hemisphere, double *out_deg)
{
    if (value == NULL || hemisphere == NULL || out_deg == NULL || value[0] == '\0' || hemisphere[0] == '\0') {
        return false;
    }

    double raw = strtod(value, NULL);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (double)degrees * 100.0;
    double result = (double)degrees + minutes / 60.0;
    if (hemisphere[0] == 'S' || hemisphere[0] == 'W') {
        result = -result;
    }

    *out_deg = result;
    return true;
}

static void handle_rmc(nmea_state_t *state, const char *line)
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
    double lat = 0.0;
    double lon = 0.0;

    (void)nmea_get_field(line, 1, utc, sizeof(utc));
    (void)nmea_get_field(line, 2, status, sizeof(status));
    (void)nmea_get_field(line, 3, lat_raw, sizeof(lat_raw));
    (void)nmea_get_field(line, 4, lat_hemi, sizeof(lat_hemi));
    (void)nmea_get_field(line, 5, lon_raw, sizeof(lon_raw));
    (void)nmea_get_field(line, 6, lon_hemi, sizeof(lon_hemi));
    (void)nmea_get_field(line, 7, speed, sizeof(speed));
    (void)nmea_get_field(line, 8, course, sizeof(course));
    (void)nmea_get_field(line, 9, date, sizeof(date));

    bool has_position = nmea_parse_lat_lon(lat_raw, lat_hemi, &lat) && nmea_parse_lat_lon(lon_raw, lon_hemi, &lon);
    if (has_position) {
        ESP_LOGI(TAG, "RMC utc=%s status=%s lat=%.6f lon=%.6f speed_kn=%s course=%s date=%s",
                 utc, status, lat, lon, speed, course, date);
    } else {
        ESP_LOGI(TAG, "RMC utc=%s status=%s speed_kn=%s course=%s date=%s", utc, status, speed, course, date);
    }

    state->saw_rmc = true;
    if (status[0] == 'A') {
        state->saw_fix = true;
    }
}

static void handle_gga(nmea_state_t *state, const char *line)
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
    double lat = 0.0;
    double lon = 0.0;

    (void)nmea_get_field(line, 1, utc, sizeof(utc));
    (void)nmea_get_field(line, 2, lat_raw, sizeof(lat_raw));
    (void)nmea_get_field(line, 3, lat_hemi, sizeof(lat_hemi));
    (void)nmea_get_field(line, 4, lon_raw, sizeof(lon_raw));
    (void)nmea_get_field(line, 5, lon_hemi, sizeof(lon_hemi));
    (void)nmea_get_field(line, 6, fix_quality, sizeof(fix_quality));
    (void)nmea_get_field(line, 7, satellites, sizeof(satellites));
    (void)nmea_get_field(line, 8, hdop, sizeof(hdop));
    (void)nmea_get_field(line, 9, altitude, sizeof(altitude));

    bool has_position = nmea_parse_lat_lon(lat_raw, lat_hemi, &lat) && nmea_parse_lat_lon(lon_raw, lon_hemi, &lon);
    if (has_position) {
        ESP_LOGI(TAG, "GGA utc=%s fix=%s sats=%s hdop=%s alt_m=%s lat=%.6f lon=%.6f",
                 utc, fix_quality, satellites, hdop, altitude, lat, lon);
    } else {
        ESP_LOGI(TAG, "GGA utc=%s fix=%s sats=%s hdop=%s alt_m=%s", utc, fix_quality, satellites, hdop, altitude);
    }

    state->saw_gga = true;
    if (atoi(fix_quality) > 0) {
        state->saw_fix = true;
    }
}

static void handle_nmea_line(nmea_state_t *state, const char *line)
{
    if (!nmea_checksum_ok(line)) {
        ESP_LOGW(TAG, "skip bad checksum: %s", line);
        return;
    }

    if (nmea_is_type(line, "RMC")) {
        handle_rmc(state, line);
    } else if (nmea_is_type(line, "GGA")) {
        handle_gga(state, line);
    }
}

static void feed_nmea(nmea_state_t *state, const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        char ch = (char)buf[i];
        if (ch == '$') {
            state->len = 0;
        }

        if (ch == '\r' || ch == '\n') {
            if (state->len > 0) {
                state->line[state->len] = '\0';
                handle_nmea_line(state, state->line);
                state->len = 0;
            }
            continue;
        }

        if (state->len < sizeof(state->line) - 1) {
            state->line[state->len++] = ch;
        } else {
            state->len = 0;
        }
    }
}

#if PRINT_RAW_NMEA
static void print_nmea(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        putchar(isprint(buf[i]) || buf[i] == '\r' || buf[i] == '\n' ? buf[i] : '.');
    }
    fflush(stdout);
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "TEST START name=gnss");
    if (!bsp_gnss_get_desc()->present) {
        ESP_LOGW(TAG, "TEST SKIP name=gnss reason=\"not present\"");
        return;
    }

    bsp_gnss_handle_t gnss = NULL;
    esp_err_t ret = bsp_gnss_open(&gnss);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=gnss step=open err=%s", esp_err_to_name(ret));
        return;
    }

    nmea_state_t nmea = {0};
    uint8_t buf[128] = {0};
    for (int i = 0; i < READ_COUNT; ++i) {
        size_t out_len = 0;
        ret = bsp_gnss_read(gnss, buf, sizeof(buf), &out_len, READ_TIMEOUT_MS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "TEST FAIL name=gnss step=read err=%s", esp_err_to_name(ret));
            goto out;
        }
        if (out_len == 0) {
            ESP_LOGI(TAG, "no data");
        } else {
#if PRINT_RAW_NMEA
            ESP_LOGI(TAG, "read %u bytes", (unsigned)out_len);
            print_nmea(buf, out_len);
#endif
            feed_nmea(&nmea, buf, out_len);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!nmea.saw_rmc && !nmea.saw_gga) {
        ESP_LOGE(TAG, "TEST FAIL name=gnss step=parse summary=\"no valid RMC/GGA sentence parsed\"");
    } else if (!nmea.saw_fix) {
        ESP_LOGI(TAG, "RMC/GGA parsed, no fix yet; this is expected indoors");
        ESP_LOGI(TAG, "TEST PASS name=gnss summary=\"RMC/GGA parsed, no fix\"");
    } else {
        ESP_LOGI(TAG, "RMC/GGA parsed and fix acquired");
        ESP_LOGI(TAG, "TEST PASS name=gnss summary=\"RMC/GGA parsed with fix\"");
    }

out:
    ret = bsp_gnss_close(gnss);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=gnss step=close err=%s", esp_err_to_name(ret));
    }
}

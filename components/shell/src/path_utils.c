#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "esp_check.h"

#include "shell_internal.h"

#define TAG "shell_path"

static bool segment_is_dot(const char *segment, size_t len)
{
    return (len == 1 && segment[0] == '.');
}

static bool segment_is_dotdot(const char *segment, size_t len)
{
    return (len == 2 && segment[0] == '.' && segment[1] == '.');
}

static esp_err_t parse_segments(const char *path,
                                const char **segments,
                                size_t *segment_lens,
                                size_t max_segments,
                                size_t *inout_count)
{
    ESP_RETURN_ON_FALSE(path != NULL, ESP_ERR_INVALID_ARG, TAG, "path is null");
    ESP_RETURN_ON_FALSE(segments != NULL, ESP_ERR_INVALID_ARG, TAG, "segments is null");
    ESP_RETURN_ON_FALSE(segment_lens != NULL, ESP_ERR_INVALID_ARG, TAG, "segment_lens is null");
    ESP_RETURN_ON_FALSE(inout_count != NULL, ESP_ERR_INVALID_ARG, TAG, "inout_count is null");
    ESP_RETURN_ON_FALSE(max_segments > 0, ESP_ERR_INVALID_ARG, TAG, "max_segments must be > 0");

    size_t segment_count = *inout_count;

    const char *p = path;
    while (*p != '\0') {
        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        const char *seg_start = p;
        while (*p != '\0' && *p != '/') {
            p++;
        }
        const size_t seg_len = (size_t)(p - seg_start);
        if (segment_is_dot(seg_start, seg_len)) {
            continue;
        }
        if (segment_is_dotdot(seg_start, seg_len)) {
            if (segment_count > 0) {
                segment_count--;
            }
            continue;
        }

        ESP_RETURN_ON_FALSE(segment_count < max_segments,
                            ESP_ERR_INVALID_SIZE, TAG, "too many path segments");
        segments[segment_count] = seg_start;
        segment_lens[segment_count] = seg_len;
        segment_count++;
    }

    *inout_count = segment_count;
    return ESP_OK;
}

static esp_err_t write_segments(const char **segments,
                                const size_t *segment_lens,
                                size_t segment_count,
                                char *out_path,
                                size_t out_size)
{
    ESP_RETURN_ON_FALSE(out_path != NULL, ESP_ERR_INVALID_ARG, TAG, "out_path is null");
    ESP_RETURN_ON_FALSE(out_size >= 2, ESP_ERR_INVALID_ARG, TAG, "out_size is too small");

    size_t out_len = 0;
    out_path[out_len++] = '/';
    for (size_t i = 0; i < segment_count; i++) {
        if (i > 0) {
            ESP_RETURN_ON_FALSE((out_len + 1) < out_size, ESP_ERR_INVALID_SIZE, TAG, "path too long");
            out_path[out_len++] = '/';
        }
        const size_t seg_len = segment_lens[i];
        ESP_RETURN_ON_FALSE((out_len + seg_len) < out_size, ESP_ERR_INVALID_SIZE, TAG, "path too long");
        memcpy(&out_path[out_len], segments[i], seg_len);
        out_len += seg_len;
    }
    out_path[out_len] = '\0';
    return ESP_OK;
}

esp_err_t shell_path_resolve(const char *cwd, const char *path, char *out_path, size_t out_size)
{
    ESP_RETURN_ON_FALSE(cwd != NULL, ESP_ERR_INVALID_ARG, TAG, "cwd is null");
    ESP_RETURN_ON_FALSE(path != NULL, ESP_ERR_INVALID_ARG, TAG, "path is null");
    ESP_RETURN_ON_FALSE(out_path != NULL, ESP_ERR_INVALID_ARG, TAG, "out_path is null");
    ESP_RETURN_ON_FALSE(cwd[0] == '/', ESP_ERR_INVALID_ARG, TAG, "cwd must be absolute");

    const char *segments[64] = {0};
    size_t segment_lens[64] = {0};
    size_t segment_count = 0;

    if (path[0] == '/') {
        ESP_RETURN_ON_ERROR(parse_segments(path, segments, segment_lens, 64, &segment_count),
                            TAG, "parse absolute path failed");
    } else {
        ESP_RETURN_ON_ERROR(parse_segments(cwd, segments, segment_lens, 64, &segment_count),
                            TAG, "parse cwd failed");
        ESP_RETURN_ON_ERROR(parse_segments(path, segments, segment_lens, 64, &segment_count),
                            TAG, "parse relative path failed");
    }

    return write_segments(segments, segment_lens, segment_count, out_path, out_size);
}

// test/touch: bsp_touch 板级自检. 只使用 BSP public API; 判据是本文件的常量.
// 除落指动作外全部由程序判定: 人工按提示落指, 程序读坐标并按容差判定对错.

#include <stdint.h>
#include <stddef.h>

#include "bsp_display.h"
#include "bsp_touch.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "touch"

static const char *TAG = "test_bsp_touch";

// 触点轮询间隔.
#define POLL_MS 10
// 单个目标点等待落指的上限; 超时判定为 fail.
#define TOUCH_TIMEOUT_MS 20000
// 等手指离开屏幕的上限.
#define RELEASE_TIMEOUT_MS 10000
// 空屏窗口: 这段时间内不应有任何触点.
#define IDLE_WINDOW_MS 1000
// 目标点半径 = 30% x min(x_max, y_max). 圆屏面板没有角落, 该半径落在中心与边缘之间.
#define TARGET_RADIUS_PERCENT 30
// 人工落点误差容限. 坐标轴镜像或交换会偏出 2 x 半径, 仍然会被判定为 fail.
#define TARGET_TOLERANCE_PX 80
// 单次 read 的缓冲; 容量大于 max_points 才能验证驱动按点数截断.
#define READ_CAPACITY 4

static bsp_touch_handle_t s_touch;

void setUp(void)
{
}

void tearDown(void)
{
    if (s_touch != NULL) {
        (void)bsp_touch_close(s_touch);
        s_touch = NULL;
    }
}

static void open_touch(void)
{
    s_touch = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_touch_open(&s_touch));
    TEST_ASSERT_NOT_NULL(s_touch);
}

// 读一次触点; 返回是否有触点, 并校验点数不超过 desc 声明的 max_points.
static bool read_point(bsp_touch_point_t *point, size_t *points_num_out)
{
    bsp_touch_point_t points[READ_CAPACITY] = {0};
    size_t points_num = 0;

    TEST_ASSERT_EQUAL(ESP_OK, bsp_touch_read(s_touch, points, READ_CAPACITY, &points_num));
    TEST_ASSERT_TRUE(points_num <= READ_CAPACITY);

    const bsp_touch_desc_t *desc = bsp_touch_get_desc();
    TEST_ASSERT_TRUE(points_num <= desc->max_points);

    if (points_num > 0) {
        *point = points[0];
    }
    *points_num_out = points_num;
    return points_num > 0;
}

static bool wait_for_touch(bsp_touch_point_t *point, int timeout_ms)
{
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline_us) {
        size_t points_num = 0;
        if (read_point(point, &points_num)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    return false;
}

static bool wait_for_release(int timeout_ms)
{
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline_us) {
        size_t points_num = 0;
        bsp_touch_point_t point = {0};
        if (!read_point(&point, &points_num)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    return false;
}

TEST_CASE("touch: open and close", "[touch]")
{
    open_touch();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_touch_close(s_touch));
    s_touch = NULL;
}

TEST_CASE("touch: second open rejected until closed", "[touch]")
{
    open_touch();

    bsp_touch_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_touch_open(&second));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_touch_close(s_touch));
    s_touch = NULL;

    open_touch();
}

TEST_CASE("touch: null arguments rejected", "[touch]")
{
    bsp_touch_point_t points[READ_CAPACITY] = {0};
    size_t points_num = 0;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_touch_open(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_touch_close(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_touch_read(NULL, points, READ_CAPACITY, &points_num));

    open_touch();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_touch_read(s_touch, NULL, READ_CAPACITY, &points_num));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_touch_read(s_touch, points, 0, &points_num));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_touch_read(s_touch, points, READ_CAPACITY, NULL));
}

TEST_CASE("touch: idle panel reports nothing", "[touch]")
{
    open_touch();

    TEST_ASSERT_TRUE_MESSAGE(wait_for_release(RELEASE_TIMEOUT_MS), "panel keeps reporting a touch; keep hands off");

    const int64_t deadline_us = esp_timer_get_time() + (int64_t)IDLE_WINDOW_MS * 1000;
    while (esp_timer_get_time() < deadline_us) {
        size_t points_num = 0;
        bsp_touch_point_t point = {0};
        TEST_ASSERT_FALSE_MESSAGE(read_point(&point, &points_num), "phantom touch on an idle panel");
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

typedef struct {
    const char *name;
    int dx;
    int dy;
} touch_target_t;

static const touch_target_t s_targets[] = {
    {"center", 0, 0},
    {"up", 0, -1},
    {"down", 0, 1},
    {"left", -1, 0},
    {"right", 1, 0},
};

TEST_CASE("touch: guided five point check", "[touch]")
{
    open_touch();

    const bsp_touch_desc_t *desc = bsp_touch_get_desc();
    const bsp_display_info_t *display = bsp_display_get_info(NULL);
    const int center_x = desc->x_max / 2;
    const int center_y = desc->y_max / 2;
    const int radius = (desc->x_max < desc->y_max ? desc->x_max : desc->y_max) * TARGET_RADIUS_PERCENT / 100;

    ESP_LOGI(TAG, "panel: display=%dx%d touch=%dx%d max_points=%u",
             display != NULL ? display->width : 0,
             display != NULL ? display->height : 0,
             desc->x_max,
             desc->y_max,
             (unsigned)desc->max_points);
    ESP_LOGI(TAG, "five taps at radius %d px, tolerance +/- %d px", radius, TARGET_TOLERANCE_PX);

    TEST_ASSERT_TRUE_MESSAGE(wait_for_release(RELEASE_TIMEOUT_MS), "panel keeps reporting a touch; keep hands off");

    for (size_t i = 0; i < sizeof(s_targets) / sizeof(s_targets[0]); i++) {
        const int target_x = center_x + s_targets[i].dx * radius;
        const int target_y = center_y + s_targets[i].dy * radius;

        ESP_LOGI(TAG, ">>> tap %s: aim at (x=%d, y=%d), hold until the next log line",
                 s_targets[i].name, target_x, target_y);

        bsp_touch_point_t point = {0};
        TEST_ASSERT_TRUE_MESSAGE(wait_for_touch(&point, TOUCH_TIMEOUT_MS), "no touch report before timeout");

        const int offset_x = (int)point.x - target_x;
        const int offset_y = (int)point.y - target_y;
        ESP_LOGI(TAG, "<<< %s: got (x=%u, y=%u), offset (%+d, %+d) px, pressure=%u, id=%u",
                 s_targets[i].name,
                 point.x,
                 point.y,
                 offset_x,
                 offset_y,
                 (unsigned)point.pressure,
                 (unsigned)point.id);

        TEST_ASSERT_INT_WITHIN(TARGET_TOLERANCE_PX, target_x, (int)point.x);
        TEST_ASSERT_INT_WITHIN(TARGET_TOLERANCE_PX, target_y, (int)point.y);

        TEST_ASSERT_TRUE_MESSAGE(wait_for_release(RELEASE_TIMEOUT_MS), "finger still on the panel");
    }
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_touch_desc_t *desc = bsp_touch_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no touch panel on this board");
        return;
    }

    selftest_run(MODULE);
}

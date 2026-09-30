// test/rtc: bsp_rtc 板级自检. 只使用 BSP public API; 判据是本文件的常量.
// 掉电走时依赖备份电池, 板子当前没装电池, 该分支无法验证, 不列入用例.

#include <stdbool.h>
#include <stdint.h>

#include "bsp_rtc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "rtc"

static const char *TAG = "test_bsp_rtc";

// set 到 get 之间的自然走时容差.
#define ROUND_TRIP_TOLERANCE_S 1
// 走时判据: 等待窗口和回读应前进的秒数区间; 上限放宽只吸收调度抖动.
#define ADVANCE_WAIT_MS 2000
#define ADVANCE_MIN_S 2
#define ADVANCE_MAX_S 5

// 探针时间是合成值, 只用于 set/get 往返自证, 不是真实时间.
// 取凌晨时刻, 远离跨天/跨月边界, 避免回读时字段翻转.
static const bsp_rtc_time_t s_probe_time = {
    .year = 2026,
    .month = 1,
    .day = 2,
    .hour = 3,
    .minute = 4,
    .second = 5,
};

static bsp_rtc_handle_t s_rtc;

void setUp(void)
{
}

void tearDown(void)
{
    if (s_rtc != NULL) {
        (void)bsp_rtc_close(s_rtc);
        s_rtc = NULL;
    }
}

static void open_rtc(void)
{
    s_rtc = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_open(&s_rtc));
    TEST_ASSERT_NOT_NULL(s_rtc);
}

static int32_t seconds_of_day(const bsp_rtc_time_t *time)
{
    return (int32_t)time->hour * 3600 + (int32_t)time->minute * 60 + time->second;
}

static int32_t second_delta(const bsp_rtc_time_t *a, const bsp_rtc_time_t *b)
{
    const int32_t diff = seconds_of_day(a) - seconds_of_day(b);
    return diff >= 0 ? diff : -diff;
}

TEST_CASE("rtc: desc is sane and open/close works", "[rtc]")
{
    const bsp_rtc_desc_t *desc = bsp_rtc_get_desc();
    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_TRUE(desc->present);

    open_rtc();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_close(s_rtc));
    s_rtc = NULL;
}

TEST_CASE("rtc: second open rejected until closed", "[rtc]")
{
    open_rtc();

    bsp_rtc_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_rtc_open(&second));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_close(s_rtc));
    s_rtc = NULL;

    open_rtc();
}

TEST_CASE("rtc: null and out-of-range arguments rejected", "[rtc]")
{
    bsp_rtc_time_t time = s_probe_time;
    bool valid = true;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_open(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_close(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_get_time(NULL, &time, &valid));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(NULL, &time));

    open_rtc();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_get_time(s_rtc, NULL, &valid));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_get_time(s_rtc, &time, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, NULL));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_set_time(s_rtc, &s_probe_time));

    bsp_rtc_time_t bad = s_probe_time;
    bad.year = 1999;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.year = 2100;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.month = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.month = 13;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.day = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.day = 32;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.hour = 24;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.minute = 60;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));
    bad = s_probe_time;
    bad.second = 60;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_rtc_set_time(s_rtc, &bad));

    // 被拒绝的写入不能改变时钟.
    bsp_rtc_time_t when = {0};
    valid = false;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_get_time(s_rtc, &when, &valid));
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_TRUE(second_delta(&when, &s_probe_time) <= ROUND_TRIP_TOLERANCE_S);
}

TEST_CASE("rtc: set/get round-trip keeps fields and clears the OS flag", "[rtc]")
{
    open_rtc();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_set_time(s_rtc, &s_probe_time));

    bsp_rtc_time_t when = {0};
    bool valid = false;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_get_time(s_rtc, &when, &valid));

    ESP_LOGI(TAG, "read back %04u-%02u-%02u %02u:%02u:%02u valid=%d",
             (unsigned)when.year, (unsigned)when.month, (unsigned)when.day,
             (unsigned)when.hour, (unsigned)when.minute, (unsigned)when.second, (int)valid);

    TEST_ASSERT_TRUE_MESSAGE(valid, "OS flag still set right after set_time");
    TEST_ASSERT_EQUAL_UINT16(s_probe_time.year, when.year);
    TEST_ASSERT_EQUAL_UINT8(s_probe_time.month, when.month);
    TEST_ASSERT_EQUAL_UINT8(s_probe_time.day, when.day);
    TEST_ASSERT_EQUAL_UINT8(s_probe_time.hour, when.hour);
    TEST_ASSERT_EQUAL_UINT8(s_probe_time.minute, when.minute);
    TEST_ASSERT_TRUE(second_delta(&when, &s_probe_time) <= ROUND_TRIP_TOLERANCE_S);
}

TEST_CASE("rtc: oscillator advances in real time", "[rtc]")
{
    open_rtc();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_set_time(s_rtc, &s_probe_time));

    bsp_rtc_time_t first = {0};
    bool valid = false;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_get_time(s_rtc, &first, &valid));
    TEST_ASSERT_TRUE(valid);

    vTaskDelay(pdMS_TO_TICKS(ADVANCE_WAIT_MS));

    bsp_rtc_time_t second = {0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_rtc_get_time(s_rtc, &second, &valid));
    TEST_ASSERT_TRUE(valid);

    const int32_t delta = second_delta(&second, &first);
    ESP_LOGI(TAG, "clock advanced %d s over %d ms", (int)delta, ADVANCE_WAIT_MS);
    TEST_ASSERT_TRUE(delta >= ADVANCE_MIN_S && delta <= ADVANCE_MAX_S);
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_rtc_desc_t *desc = bsp_rtc_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no rtc on this board");
        return;
    }

    selftest_run(MODULE);
}

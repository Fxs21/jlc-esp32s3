// test/key: bsp_key 板级自检. 只使用 BSP public API; 判据是本文件的常量.
// 键是 GPIO0 直连的 BOOT/用户键, 按下为低; 本模块只承诺瞬时电平,
// 不去抖, 无事件, 自检也只验证电平跟随按键变化.

#include <stdbool.h>

#include "bsp_key.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "key"

static const char *TAG = "test_bsp_key";

// 引导动作窗口: 按提示按下/松开后, 程序轮询到这个窗口结束.
#define GUIDED_TIMEOUT_MS 30000
// 轮询间隔; 抖动由调用方自己处理, 这里只需要足够小的采样步长.
#define POLL_MS 20
// 引导开始前等待键处于松开状态的窗口.
#define RELEASE_TIMEOUT_MS 5000

static bsp_key_handle_t s_key;

void setUp(void)
{
}

void tearDown(void)
{
    if (s_key != NULL) {
        (void)bsp_key_close(s_key);
        s_key = NULL;
    }
}

static void open_key(void)
{
    s_key = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_key_open(&s_key));
    TEST_ASSERT_NOT_NULL(s_key);
}

static bool is_pressed(void)
{
    bool pressed = false;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_key_is_pressed(s_key, &pressed));
    return pressed;
}

static bool wait_for_level(bool want_pressed, int timeout_ms)
{
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline_us) {
        if (is_pressed() == want_pressed) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    return is_pressed() == want_pressed;
}

TEST_CASE("key: desc is sane and open/close works", "[key]")
{
    const bsp_key_desc_t *desc = bsp_key_get_desc();
    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_TRUE(desc->present);

    open_key();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_key_close(s_key));
    s_key = NULL;
}

TEST_CASE("key: second open rejected until closed", "[key]")
{
    open_key();

    bsp_key_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_key_open(&second));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_key_close(s_key));
    s_key = NULL;

    open_key();
}

TEST_CASE("key: null arguments rejected", "[key]")
{
    bool pressed = false;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_key_open(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_key_close(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_key_is_pressed(NULL, &pressed));

    open_key();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_key_is_pressed(s_key, NULL));
}

TEST_CASE("key: guided press and release follows the level", "[key]")
{
    open_key();

    TEST_ASSERT_TRUE_MESSAGE(wait_for_level(false, RELEASE_TIMEOUT_MS),
                             "key reads pressed before the guided step; release the BOOT key");

    ESP_LOGI(TAG, ">>> press and hold the BOOT key now");
    const bool pressed = wait_for_level(true, GUIDED_TIMEOUT_MS);
    if (!pressed) {
        ESP_LOGE(TAG, "no press detected within %d ms", GUIDED_TIMEOUT_MS);
    }
    TEST_ASSERT_TRUE_MESSAGE(pressed, "pressed level not detected; key or GPIO0 read is broken");
    ESP_LOGI(TAG, "<<< press detected; release the BOOT key now");

    const bool released = wait_for_level(false, GUIDED_TIMEOUT_MS);
    if (!released) {
        ESP_LOGE(TAG, "no release detected within %d ms", GUIDED_TIMEOUT_MS);
    }
    TEST_ASSERT_TRUE_MESSAGE(released, "released level not detected; key reads stuck pressed");
    ESP_LOGI(TAG, "<<< release detected");
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_key_desc_t *desc = bsp_key_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no GPIO user key on this board");
        return;
    }

    selftest_run(MODULE);
}

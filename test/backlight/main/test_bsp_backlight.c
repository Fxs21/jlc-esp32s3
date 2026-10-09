// test/backlight: bsp_backlight 板级自检. 只使用 BSP public API; 判据和档位是本文件的常量.
// 自动项: desc, open/close, 二次 open 拒绝, 参数校验, percent clamp 与往返.
// 人工项: 亮度分档是否可见, 100% 是否恢复.
// 前置: AuraS3 的背光是 CO5300 亮度代理, 必须先 bsp_display_open(); 本 app 打开 display
//       并把屏幕填白作为观察背景. display 自己的判据在 test/display, 这里只当观察面.

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsp_backlight.h"
#include "bsp_board.h"
#include "bsp_display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "backlight"

static const char *TAG = "test_bsp_backlight";

// 填背景用的横条高度: 466 x 16 x 2 B = 14912 B, 不占大缓冲.
#define STRIP_LINES 16
// 每个档位的停留时间, 让人看清变化.
#define STEP_HOLD_MS 1500
// 人工确认窗口.
#define HUMAN_TIMEOUT_MS 30000
// 等传输完成回调的上限 (fill_screen 是异步写).
#define DONE_TIMEOUT_MS 3000

// host order 的 RGB565; 面板字节序在写之前才转换.
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

static bsp_display_handle_t s_display;
static bsp_backlight_handle_t s_backlight;

static volatile uint32_t s_done_count;

static bool IRAM_ATTR on_transfer_done(void *user_ctx)
{
    (void)user_ctx;
    s_done_count++;
    return false;
}

// 面板原生字节序, 见 docs/hw/boards/<board>/truth_table.md (与 test/display 同源).
static uint16_t panel_color(uint16_t rgb565)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    const bool high_byte_first = (board != NULL && board->id == BSP_BOARD_ID_AURAS3);
    return high_byte_first ? __builtin_bswap16(rgb565) : rgb565;
}

void setUp(void)
{
}

void tearDown(void)
{
    if (s_backlight != NULL) {
        (void)bsp_backlight_close(s_backlight);
        s_backlight = NULL;
    }
    if (s_display != NULL) {
        // 完成回调在 ISR 里调用, 释放 handle 之前先摘掉.
        (void)bsp_display_set_done_cb(s_display, NULL, NULL);
        (void)bsp_display_close(s_display);
        s_display = NULL;
    }
}

static bool wait_done_count(uint32_t target)
{
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)DONE_TIMEOUT_MS * 1000;
    while (esp_timer_get_time() < deadline_us) {
        if (s_done_count >= target) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

// 背光的观察面: 屏幕整屏填色.
// bsp_display_write() 是异步的: strip 要等这一轮发出的传输全部回调完成再释放.
static esp_err_t fill_screen(uint16_t color)
{
    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    if (info == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t strip_px = (size_t)info->width * STRIP_LINES;
    uint16_t *strip = malloc(strip_px * sizeof(uint16_t));
    if (strip == NULL) {
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < strip_px; ++i) {
        strip[i] = color;
    }

    const uint32_t base = s_done_count;
    uint32_t issued = 0;
    esp_err_t ret = ESP_OK;
    uint16_t done = 0;
    while (done < info->height) {
        const uint16_t lines = ((uint16_t)(info->height - done) < STRIP_LINES) ? (uint16_t)(info->height - done)
                                                                             : STRIP_LINES;
        ret = bsp_display_write(s_display, 0, (uint16_t)(done), info->width, lines, strip,
                                (size_t)info->width * lines * sizeof(uint16_t));
        if (ret != ESP_OK) {
            break;
        }
        issued++;
        done = (uint16_t)(done + lines);
    }

    const bool drained = wait_done_count(base + issued);
    free(strip);
    if (ret != ESP_OK) {
        return ret;
    }
    return drained ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void open_display(void)
{
    s_display = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_open(&s_display));
    // 所有写都是异步的: 统一挂上 done 计数, fill_screen 靠它等传输完成.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_set_done_cb(s_display, on_transfer_done, NULL));
}

static void open_backlight(void)
{
    s_backlight = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_open(&s_backlight));
    TEST_ASSERT_NOT_NULL(s_backlight);
}

// 人工项: y = 通过, n = 失败, 超时/无输入 = pending, 既不算通过也不算失败.
static void human_check(const char *item, const char *prompt)
{
    const int answer = selftest_human_check(MODULE, item, prompt, HUMAN_TIMEOUT_MS);
    if (answer < 0) {
        TEST_IGNORE_MESSAGE("no answer within the window: human item pending");
    }
    TEST_ASSERT_EQUAL_MESSAGE(1, answer, "operator reported that the brightness is wrong");
}

TEST_CASE("backlight: desc is sane and open/close works", "[backlight]")
{
    const bsp_backlight_desc_t *desc = bsp_backlight_get_desc();
    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_TRUE(desc->present);

    open_display();
    open_backlight();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_close(s_backlight));
    s_backlight = NULL;
}

TEST_CASE("backlight: second open rejected until closed", "[backlight]")
{
    open_display();
    open_backlight();

    bsp_backlight_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_backlight_open(&second));
    TEST_ASSERT_NULL(second);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_close(s_backlight));
    s_backlight = NULL;

    open_backlight();
}

TEST_CASE("backlight: null arguments rejected", "[backlight]")
{
    uint8_t percent = 0;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_backlight_open(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_backlight_close(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_backlight_set_percent(NULL, 50));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_backlight_get_percent(NULL, &percent));

    open_display();
    open_backlight();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_backlight_get_percent(s_backlight, NULL));
}

TEST_CASE("backlight: percent round-trip and clamp", "[backlight]")
{
    open_display();
    open_backlight();

    static const uint8_t steps[] = {0, 10, 50, 100};
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i) {
        TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_set_percent(s_backlight, steps[i]));
        uint8_t percent = 0xFF;
        TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_get_percent(s_backlight, &percent));
        TEST_ASSERT_EQUAL_UINT8(steps[i], percent);
    }

    // 超过 100 的取值按 API 约定收敛到 100.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_set_percent(s_backlight, 200));
    uint8_t percent = 0;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_get_percent(s_backlight, &percent));
    TEST_ASSERT_EQUAL_UINT8(100, percent);
}

TEST_CASE("backlight: brightness steps are visible", "[backlight]")
{
    static const uint8_t steps[] = {100, 50, 10, 0};

    open_display();
    open_backlight();
    TEST_ASSERT_EQUAL(ESP_OK, fill_screen(panel_color(RGB565(255, 255, 255))));

    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i) {
        ESP_LOGI(TAG, ">>> brightness: %u%%", (unsigned)steps[i]);
        TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_set_percent(s_backlight, steps[i]));
        vTaskDelay(pdMS_TO_TICKS(STEP_HOLD_MS));
    }

    human_check("brightness-steps",
                "did the white screen dim step by step (100%, 50%, 10%) and look black at 0%?");
}

TEST_CASE("backlight: full brightness restores", "[backlight]")
{
    open_display();
    open_backlight();
    TEST_ASSERT_EQUAL(ESP_OK, fill_screen(panel_color(RGB565(255, 255, 255))));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_set_percent(s_backlight, 0));
    vTaskDelay(pdMS_TO_TICKS(STEP_HOLD_MS));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_set_percent(s_backlight, 100));
    vTaskDelay(pdMS_TO_TICKS(STEP_HOLD_MS));

    human_check("restore", "is the white screen back to full brightness after 0% -> 100%?");
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_backlight_desc_t *desc = bsp_backlight_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no backlight on this board");
        return;
    }

    selftest_run(MODULE);
}

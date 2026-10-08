// test/ui: bsp_ui 板级自检: LVGL 显示 + 触摸 + 背光的组合通路.
// 只使用 BSP public API; 判据和窗口参数是本文件的常量.
// 自动项: 组合 open/close, LVGL display/indev 绑定, 分辨率, handle 独占, 参数校验, process 延时, 刷帧.
// 人工项: 内置 widgets demo 显示完整, 触摸跟手 (画面来源是 LVGL 自带 demo, 不是自建界面).
//
// ui 全程只开一次 (setUp 里开, 跑到结束): 每次 open 都会给面板一次硬件 reset,
// 亮度 0 的黑屏上能看见一次亮闪, 所以除生命周期那条外不再重复开关.

#include <stdbool.h>
#include <stdint.h>

#include "bsp_backlight.h"
#include "bsp_display.h"
#include "bsp_touch.h"
#include "bsp_ui.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_demos.h"
#include "lvgl.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "ui"

static const char *TAG = "test_bsp_ui";

// 单帧之间的最大睡眠: bsp_ui_process() 返回的是"下次该跑的时间", 太长界面会发呆.
#define MAX_TICK_MS 20
// process 延时提示的上限: 超过这个值说明返回值和 vTaskDelay() 的口径对不上.
#define MAX_DELAY_HINT_MS 100
// 采样 process 延时提示的时长.
#define DELAY_SAMPLE_MS 1000
// 等第一帧刷出去的窗口.
#define FLUSH_WINDOW_MS 3000
// 点背光之前先让首帧进面板的时间.
#define FIRST_FRAME_MS 400
// 人动手的窗口: 期间界面一直在动, 可以拖 slider, 切 tab.
#define INTERACT_MS 20000
// 人工确认窗口.
#define HUMAN_TIMEOUT_MS 30000

static bsp_ui_handle_t s_ui;
static volatile uint32_t s_flush_count;

static bool touch_present(void)
{
    const bsp_touch_desc_t *desc = bsp_touch_get_desc();
    return desc != NULL && desc->present;
}

static bool backlight_present(void)
{
    const bsp_backlight_desc_t *desc = bsp_backlight_get_desc();
    return desc != NULL && desc->present;
}

static void open_ui(void)
{
    s_ui = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_ui_open(&s_ui));
    TEST_ASSERT_NOT_NULL(s_ui);
}

// 每个用例都复用一个 ui; 只有生命周期那条自己关, 关完再开回来.
void setUp(void)
{
    if (s_ui == NULL) {
        open_ui();
    }
}

void tearDown(void)
{
}

// 跑一帧 LVGL: bsp_ui_process() 的返回值直接当睡眠时长, 但截到上限.
// LV_NO_TIMER_READY 是"当前没有定时器"的正常返回, 按上限睡, 不算错误.
static void lvgl_tick(void)
{
    uint32_t delay_ms = 0;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_ui_process(s_ui, &delay_ms));
    if (delay_ms == LV_NO_TIMER_READY || delay_ms > MAX_TICK_MS) {
        delay_ms = MAX_TICK_MS;
    }
    if (delay_ms == 0) {
        delay_ms = 1;
    }
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

static void lvgl_run(uint32_t duration_ms)
{
    const int64_t deadline = esp_timer_get_time() + (int64_t)duration_ms * 1000;
    while (esp_timer_get_time() < deadline) {
        lvgl_tick();
    }
}

static void human_check(const char *item, const char *prompt)
{
    const int answer = selftest_human_check(MODULE, item, prompt, HUMAN_TIMEOUT_MS);
    if (answer < 0) {
        TEST_IGNORE_MESSAGE("no answer within the window: human item pending");
    }
    TEST_ASSERT_EQUAL_MESSAGE(1, answer, "operator reported that the screen is wrong");
}

// LVGL 把一帧交给 flush_cb, 等它完成 (面板 DMA 走完) 才发 FLUSH_FINISH; 用它证明帧真的到了面板.
// REFR_READY 不能用: 没有重绘时它也会发.
static void on_display_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_FLUSH_FINISH) {
        s_flush_count++;
    }
}

TEST_CASE("ui: open binds an LVGL display, an indev and the backlight", "[ui]")
{
    lv_display_t *display = bsp_ui_get_lvgl_display(s_ui);
    TEST_ASSERT_NOT_NULL(display);

    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(info->width, lv_display_get_horizontal_resolution(display));
    TEST_ASSERT_EQUAL_INT(info->height, lv_display_get_vertical_resolution(display));

    // 有触摸就该有 indev; 没有触摸的板子上 indev 是 NULL 才对.
    lv_indev_t *indev = bsp_ui_get_lvgl_indev(s_ui);
    if (touch_present()) {
        TEST_ASSERT_NOT_NULL(indev);
    } else {
        TEST_ASSERT_NULL(indev);
    }

    TEST_ASSERT_TRUE(backlight_present());
}

TEST_CASE("ui: ui owns the native display and backlight handles", "[ui]")
{
    // ui 内部已经拿了 display 和 backlight, 这两个 handle 同时只能有一个持有者.
    bsp_ui_handle_t second_ui = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_ui_open(&second_ui));
    TEST_ASSERT_NULL(second_ui);

    bsp_display_handle_t display = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_display_open(&display));
    TEST_ASSERT_NULL(display);

    bsp_backlight_handle_t backlight = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_backlight_open(&backlight));
    TEST_ASSERT_NULL(backlight);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_ui_close(s_ui));
    s_ui = NULL;

    // ui 关掉之后原生 handle 要能拿回来 (display / backlight 引用计数回归).
    // 顺序固定: 背光依附于已经打开的显示.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_open(&display));
    TEST_ASSERT_NOT_NULL(display);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_open(&backlight));
    TEST_ASSERT_NOT_NULL(backlight);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_close(backlight));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_close(display));

    // 后面的用例还要用, 开回来.
    open_ui();
}

TEST_CASE("ui: null arguments are rejected", "[ui]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_ui_open(NULL));

    uint32_t delay_ms = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_ui_process(NULL, &delay_ms));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_ui_process(s_ui, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_ui_set_backlight(NULL, 50));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_ui_close(NULL));
    TEST_ASSERT_NULL(bsp_ui_get_lvgl_display(NULL));
    TEST_ASSERT_NULL(bsp_ui_get_lvgl_indev(NULL));
}

TEST_CASE("ui: process returns a usable delay hint", "[ui]")
{
    const int64_t deadline = esp_timer_get_time() + (int64_t)DELAY_SAMPLE_MS * 1000;
    uint32_t finite_hints = 0;
    while (esp_timer_get_time() < deadline) {
        uint32_t delay_ms = 0;
        TEST_ASSERT_EQUAL(ESP_OK, bsp_ui_process(s_ui, &delay_ms));
        if (delay_ms != LV_NO_TIMER_READY) {
            // 这个值会被 app 直接交给 vTaskDelay(), 必须是正常的毫秒数.
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(MAX_DELAY_HINT_MS, delay_ms);
            finite_hints++;
        }
        vTaskDelay(pdMS_TO_TICKS(MAX_TICK_MS));
    }

    // 空屏也有 LVGL 自己的刷新定时器, 不该整段时间只返回 LV_NO_TIMER_READY.
    TEST_ASSERT_GREATER_THAN_UINT32(0, finite_hints);
}

TEST_CASE("ui: frames reach the panel", "[ui]")
{
    lv_display_t *display = bsp_ui_get_lvgl_display(s_ui);
    TEST_ASSERT_NOT_NULL(display);

    s_flush_count = 0;
    lv_display_add_event_cb(display, on_display_event, LV_EVENT_ALL, NULL);

    // 逼一次重绘: 这里要的是"注册之后还有帧走出去".
    lv_obj_invalidate(lv_screen_active());

    const int64_t deadline = esp_timer_get_time() + (int64_t)FLUSH_WINDOW_MS * 1000;
    while (s_flush_count == 0 && esp_timer_get_time() < deadline) {
        lvgl_tick();
    }

    TEST_ASSERT_GREATER_THAN_UINT32(0, s_flush_count);
}

TEST_CASE("ui: widgets demo renders and follows touch", "[ui]")
{
    // 画面来源是 LVGL 自带 demo, 不是自建界面; 三个 demo 随时可换.
    // 换 demo 时同步改 test/ui/sdkconfig.defaults 里对应的 CONFIG_LV_USE_DEMO_*.
    // demo 字号按屏宽自动分档, sdkconfig.defaults 开的是会走到的两档 (12/18, 14/20).
    // lv_demo_benchmark();
    lv_demo_widgets();
    // lv_demo_stress();

    // 首帧先进面板再点灯: 亮度 0 起画面更干净 (truth table §4: UI 首帧后再开亮度).
    lvgl_run(FIRST_FRAME_MS);
    if (backlight_present()) {
        TEST_ASSERT_EQUAL(ESP_OK, bsp_ui_set_backlight(s_ui, 100));
    }

    ESP_LOGI(TAG, ">>> widgets demo: drag a slider, switch a tab; you will be asked in %d s", INTERACT_MS / 1000);
    lvgl_run(INTERACT_MS);

    human_check("widgets-demo",
                "did the demo render completely (colors right, no garbage) and did touch follow your finger?");
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    if (info == NULL || !info->present) {
        selftest_skip(MODULE, "no display on this board");
        return;
    }

    selftest_run(MODULE);
}

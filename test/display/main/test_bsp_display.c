// test/display: bsp_display 板级自检. 只使用 BSP public API; 判据和图案参数是本文件的常量.
// 自动项: desc, open/close, 参数校验, 异步传输完成回调计数, 分块写入.
// 人工项: 全屏颜色顺序, 居中对称图案, 左边缘标记 (验证 CO5300 列偏移).
// 可见性前提: 本 app 顺带把 backlight 打开并设为 100%; backlight 自己的判据在 test/backlight.

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

#define MODULE "display"

static const char *TAG = "test_bsp_display";

// 分块写入的行数: 466 x 16 x 2 B = 14912 B, 不占大缓冲, 不冒栈上限的风险.
#define STRIP_LINES 16
// 每屏颜色停留时间, 让人看清切换顺序.
#define COLOR_HOLD_MS 1200
// 等传输完成回调的上限.
#define DONE_TIMEOUT_MS 3000
// 人工确认窗口. 人看到的是屏幕而不是串口滚动, 给足时间.
#define HUMAN_TIMEOUT_MS 30000
// 居中图案: 方框边长 = min(width, height) 的 60%, 十字长度 40%, 线宽 4 px.
#define FRAME_PERCENT 60
#define CROSS_PERCENT 40
#define LINE_THICKNESS 4
// 左边缘标记: 宽度等于 CO5300 的 x gap (0x06), 列偏移错位时它不会贴住左边缘.
#define EDGE_MARK_WIDTH 6
#define EDGE_MARK_HEIGHT 40

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

// 面板原生字节序, 见 docs/hw/boards/<board>/truth_table.md:
//   AuraS3 CO5300 native stream 是 high-byte-first RGB565, 写入前要 bswap;
//   DoerS3 ST7789 native contract 是 little-endian RGB565, 不转换.
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

static void open_display(void)
{
    s_display = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_open(&s_display));
    TEST_ASSERT_NOT_NULL(s_display);
    // 所有写都是异步的: 统一挂上 done 计数, fill_rect 靠它等传输完成.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_set_done_cb(s_display, on_transfer_done, NULL));
}

// 背光是"能看见画面"的前提, 不是本 app 的被测对象.
static void open_backlight(void)
{
    const bsp_backlight_desc_t *desc = bsp_backlight_get_desc();
    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_TRUE_MESSAGE(desc->present, "no backlight on this board; the panel stays dark");

    s_backlight = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_open(&s_backlight));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_backlight_set_percent(s_backlight, 100));
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

// 用固定高度的横条填一块矩形: 面积再大也只占 width x STRIP_LINES 的缓冲.
// bsp_display_write() 是异步的: strip 要等这一轮发出的传输全部回调完成再释放,
// 否则队列里 pending 的横条会读到下一块图案复用后的内存, 真机上表现为底部残留色条.
static esp_err_t fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    const size_t strip_px = (size_t)width * STRIP_LINES;
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
    while (done < height) {
        const uint16_t lines = ((uint16_t)(height - done) < STRIP_LINES) ? (uint16_t)(height - done) : STRIP_LINES;
        ret = bsp_display_write(s_display, x, (uint16_t)(y + done), width, lines, strip,
                                (size_t)width * lines * sizeof(uint16_t));
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

// 人工项: y = 通过, n = 失败, 超时/无输入 = pending, 既不算通过也不算失败.
static void human_check(const char *item, const char *prompt)
{
    const int answer = selftest_human_check(MODULE, item, prompt, HUMAN_TIMEOUT_MS);
    if (answer < 0) {
        TEST_IGNORE_MESSAGE("no answer within the window: human item pending");
    }
    TEST_ASSERT_EQUAL_MESSAGE(1, answer, "operator reported that the pattern is wrong");
}

TEST_CASE("display: desc is sane and open/close works", "[display]")
{
    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_TRUE(info->present);
    TEST_ASSERT_TRUE(info->width > 0);
    TEST_ASSERT_TRUE(info->height > 0);
    TEST_ASSERT_EQUAL_UINT8(16, info->bits_per_pixel);
    ESP_LOGI(TAG, "panel: %ux%u %u bpp", (unsigned)info->width, (unsigned)info->height,
             (unsigned)info->bits_per_pixel);

    open_display();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_close(s_display));
    s_display = NULL;
}

TEST_CASE("display: second open rejected until closed", "[display]")
{
    open_display();

    bsp_display_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_display_open(&second));
    TEST_ASSERT_NULL(second);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_close(s_display));
    s_display = NULL;

    open_display();
}

TEST_CASE("display: null and out-of-range arguments rejected", "[display]")
{
    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    uint16_t pixels[STRIP_LINES * 4] = {0};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_open(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_close(NULL));

    open_display();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_write(NULL, 0, 0, 2, 2, pixels, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_write(s_display, 0, 0, 2, 2, NULL, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_write(s_display, 0, 0, 0, 2, pixels, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_write(s_display, 0, 0, 2, 0, pixels, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_write(s_display, info->width, 0, 2, 2, pixels, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_write(s_display, 0, info->height, 2, 2, pixels, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_display_write(s_display, (uint16_t)(info->width - 1), 0, 2, 2, pixels, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bsp_display_write(s_display, 0, (uint16_t)(info->height - 1), 2, 2, pixels, 8));
    // data_size 不足: 4 x 4 RGB565 需要 32 B.
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_display_write(s_display, 0, 0, 4, 4, pixels, 30));
    // 合法的最小写入. pixels 是栈上缓冲, 等传输完成再离开本用例.
    const uint32_t base = s_done_count;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_display_write(s_display, 0, 0, 2, 2, pixels, 8));
    TEST_ASSERT_TRUE(wait_done_count(base + 1));
}

TEST_CASE("display: done callback fires once per queued write", "[display]")
{
    open_display();

    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    const uint32_t writes = ((uint32_t)info->height + STRIP_LINES - 1) / STRIP_LINES;

    s_done_count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, fill_rect(0, 0, info->width, info->height, panel_color(RGB565(0, 0, 0))));
    TEST_ASSERT_TRUE_MESSAGE(wait_done_count(writes), "transfer done callback did not reach the expected count");

    // 多出来的回调会在这段时间里暴露.
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL_UINT32(writes, s_done_count);
}

TEST_CASE("display: full screen color sweep", "[display]")
{
    open_display();
    open_backlight();

    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    static const struct {
        const char *name;
        uint16_t rgb565;
    } colors[] = {
        {"red", RGB565(255, 0, 0)},
        {"green", RGB565(0, 255, 0)},
        {"blue", RGB565(0, 0, 255)},
        {"white", RGB565(255, 255, 255)},
        {"black", RGB565(0, 0, 0)},
    };

    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        ESP_LOGI(TAG, ">>> full screen: %s", colors[i].name);
        TEST_ASSERT_EQUAL(ESP_OK, fill_rect(0, 0, info->width, info->height, panel_color(colors[i].rgb565)));
        vTaskDelay(pdMS_TO_TICKS(COLOR_HOLD_MS));
    }

    human_check("color-sweep", "did the whole screen show red, green, blue, white, black in this order?");
}

TEST_CASE("display: centered frame and cross", "[display]")
{
    open_display();
    open_backlight();

    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    const uint16_t width = info->width;
    const uint16_t height = info->height;
    const uint16_t min_side = (width < height) ? width : height;
    const uint16_t frame = (uint16_t)(min_side * FRAME_PERCENT / 100);
    const uint16_t cross = (uint16_t)(min_side * CROSS_PERCENT / 100);
    const uint16_t left = (uint16_t)((width - frame) / 2);
    const uint16_t top = (uint16_t)((height - frame) / 2);
    const uint16_t center_x = (uint16_t)(width / 2);
    const uint16_t center_y = (uint16_t)(height / 2);
    const uint16_t white = panel_color(RGB565(255, 255, 255));

    TEST_ASSERT_EQUAL(ESP_OK, fill_rect(0, 0, width, height, panel_color(RGB565(0, 0, 0))));
    TEST_ASSERT_EQUAL(ESP_OK, fill_rect(left, top, frame, LINE_THICKNESS, white));
    TEST_ASSERT_EQUAL(ESP_OK, fill_rect(left, (uint16_t)(top + frame - LINE_THICKNESS), frame, LINE_THICKNESS, white));
    TEST_ASSERT_EQUAL(ESP_OK, fill_rect(left, top, LINE_THICKNESS, frame, white));
    TEST_ASSERT_EQUAL(ESP_OK, fill_rect((uint16_t)(left + frame - LINE_THICKNESS), top, LINE_THICKNESS, frame, white));
    TEST_ASSERT_EQUAL(ESP_OK, fill_rect((uint16_t)(center_x - cross / 2), (uint16_t)(center_y - LINE_THICKNESS / 2),
                                        cross, LINE_THICKNESS, white));
    TEST_ASSERT_EQUAL(ESP_OK, fill_rect((uint16_t)(center_x - LINE_THICKNESS / 2), (uint16_t)(center_y - cross / 2),
                                        LINE_THICKNESS, cross, white));

    human_check("frame-centered",
                "is the cross in the middle of the panel and the square frame symmetric on all sides?");
}

TEST_CASE("display: left edge marker reaches the panel edge", "[display]")
{
    open_display();
    open_backlight();

    const bsp_display_info_t *info = bsp_display_get_info(NULL);
    const uint16_t mark_y = (uint16_t)((info->height - EDGE_MARK_HEIGHT) / 2);

    TEST_ASSERT_EQUAL(ESP_OK, fill_rect(0, 0, info->width, info->height, panel_color(RGB565(0, 0, 0))));
    TEST_ASSERT_EQUAL(ESP_OK,
                      fill_rect(0, mark_y, EDGE_MARK_WIDTH, EDGE_MARK_HEIGHT, panel_color(RGB565(255, 0, 0))));

    human_check("edge-marker", "is there a red mark touching the left edge of the panel at mid height?");
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

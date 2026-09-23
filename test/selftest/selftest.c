#include "selftest.h"

#include <stdint.h>

#include "bsp_board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include "unity.h"

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag.h"
#elif CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
#include "driver/uart.h"
#endif

#ifndef TEST_GIT_SHA
#error "TEST_GIT_SHA is not defined; build through test/bsp.sh"
#endif

static const char *TAG = "selftest";

const char selftest_git_sha[] = TEST_GIT_SHA;

static const char *board_name(void)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    return (board != NULL && board->name != NULL) ? board->name : "unknown";
}

void selftest_start(const char *module)
{
    ESP_LOGI(TAG, "SELFTEST start module=%s board=%s sha=%s", module, board_name(), selftest_git_sha);
}

bool selftest_run(const char *module)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();

    const unsigned tests = (unsigned)Unity.NumberOfTests;
    const unsigned failed = (unsigned)Unity.TestFailures;
    ESP_LOGI(TAG, "SELFTEST end module=%s board=%s sha=%s tests=%u failed=%u result=%s",
             module, board_name(), selftest_git_sha, tests, failed, failed == 0 ? "PASS" : "FAIL");
    return failed == 0;
}

void selftest_skip(const char *module, const char *reason)
{
    ESP_LOGI(TAG, "SELFTEST end module=%s board=%s sha=%s result=SKIP reason=%s",
             module, board_name(), selftest_git_sha, reason != NULL ? reason : "unspecified");
}

static bool s_input_ready;
static bool s_input_available = true;

static void console_input_init(void)
{
    if (s_input_ready) {
        return;
    }
    s_input_ready = true;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    const esp_err_t err = usb_serial_jtag_driver_install(&cfg);
#elif CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
    // tx_buffer_size = 0: 不接管 console 的 TX 路径.
    const esp_err_t err = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
#else
    const esp_err_t err = ESP_FAIL;
#endif

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "console input unavailable: %s", esp_err_to_name(err));
        s_input_available = false;
    }
}

static int console_read_byte(int timeout_ms)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    uint8_t c = 0;
    return usb_serial_jtag_read_bytes(&c, 1, pdMS_TO_TICKS(timeout_ms)) == 1 ? (int)c : -1;
#elif CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
    uint8_t c = 0;
    return uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, 1, pdMS_TO_TICKS(timeout_ms)) == 1 ? (int)c : -1;
#else
    (void)timeout_ms;
    return -1;
#endif
}

int selftest_ask_yes_no(const char *prompt, int timeout_ms)
{
    console_input_init();
    if (!s_input_available) {
        return -1;
    }

    ESP_LOGI(TAG, "%s (y/n, %d ms)", prompt != NULL ? prompt : "confirm", timeout_ms);

    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    int answer = -1;
    while (esp_timer_get_time() < deadline_us) {
        const int c = console_read_byte(100);
        if (c == 'y' || c == 'Y') {
            answer = 1;
            break;
        }
        if (c == 'n' || c == 'N') {
            answer = 0;
            break;
        }
    }
    return answer;
}

bool selftest_human_check(const char *module, const char *item, const char *prompt, int timeout_ms)
{
    const int answer = selftest_ask_yes_no(prompt, timeout_ms);
    const char *result = (answer == 1) ? "yes" : (answer == 0) ? "no" : "pending";
    ESP_LOGI(TAG, "SELFTEST human module=%s board=%s item=%s result=%s",
             module, board_name(), item, result);
    return answer == 1;
}

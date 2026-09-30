// test/shell: 验证 shell 能起来 (repl + 命令表), 不做任何硬件验收.
// shell 的硬件调试命令保留给人交互使用, 模块结论不看 shell 输出.
#include "bsp_board.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "shell.h"
#include "unity.h"

#define MODULE "shell"

static const char *TAG = "test_shell";
static shell_cfg_t s_cfg;

TEST_CASE("shell: mount table accepts an absolute path before init", "[shell]")
{
    TEST_ASSERT_EQUAL(ESP_OK, shell_mount_add("/sdcard"));
}

TEST_CASE("shell: init starts the repl", "[shell]")
{
    s_cfg = shell_default_config();
    s_cfg.initial_path = "/";
    s_cfg.enable_bsp_commands = true;
    TEST_ASSERT_EQUAL(ESP_OK, shell_init(&s_cfg));
}

TEST_CASE("shell: second init is rejected", "[shell]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, shell_init(&s_cfg));
}

TEST_CASE("shell: registered bsp command runs", "[shell]")
{
    int rc = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_console_run("bsp info", &rc));
    TEST_ASSERT_EQUAL(0, rc);
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_board_info_t *board = bsp_board_get_info();
    ESP_LOGI(TAG, "BSP board: %s", board != NULL ? board->name : "unknown");

    (void)selftest_run(MODULE);

    // 自检结束后 shell 继续可用, 供人工调试.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

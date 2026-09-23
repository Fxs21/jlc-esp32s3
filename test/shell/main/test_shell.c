#include "bsp_board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shell.h"

static const char *TAG = "test_shell";

void app_main(void)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    ESP_LOGI(TAG, "BSP board: %s", board != NULL ? board->name : "unknown");

    // The mount table is read-only after shell_init(), so the path is
    // registered here; the shell "sd" command owns the card lifecycle.
    esp_err_t ret = shell_mount_add("/sdcard");
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "shell_mount_add failed: %s", esp_err_to_name(ret));
    }

    shell_cfg_t shell_cfg = shell_default_config();
    shell_cfg.initial_path = "/";
    shell_cfg.enable_bsp_commands = true;
    ret = shell_init(&shell_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "shell_init failed: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#include <stdbool.h>

#include "bsp_board.h"
#include "bsp_sdcard.h"
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

    bsp_sdcard_handle_t sdcard = NULL;
    bool sdcard_mounted = false;

    esp_err_t ret = bsp_sdcard_open(&sdcard);
    if (ret == ESP_OK) {
        ret = bsp_sdcard_mount(sdcard, "/sdcard");
        if (ret == ESP_OK) {
            ret = shell_mount_add("/sdcard");
            if (ret == ESP_OK) {
                sdcard_mounted = true;
            } else {
                ESP_LOGW(TAG, "shell_mount_add failed: %s", esp_err_to_name(ret));
                (void)bsp_sdcard_unmount(sdcard);
            }
        } else {
            ESP_LOGW(TAG, "bsp_sdcard_mount failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "bsp_sdcard_open failed: %s; starting shell without /sdcard", esp_err_to_name(ret));
    }

    if (!sdcard_mounted && sdcard != NULL) {
        (void)bsp_sdcard_close(sdcard);
        sdcard = NULL;
    }

    shell_cfg_t shell_cfg = shell_default_config();
    shell_cfg.initial_path = sdcard_mounted ? "/sdcard" : "/";
    shell_cfg.enable_bsp_commands = true;
    ret = shell_init(&shell_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "shell_init failed: %s", esp_err_to_name(ret));
        if (sdcard_mounted) {
            (void)shell_mount_remove("/sdcard");
            (void)bsp_sdcard_unmount(sdcard);
        }
        if (sdcard != NULL) {
            (void)bsp_sdcard_close(sdcard);
        }
        return;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

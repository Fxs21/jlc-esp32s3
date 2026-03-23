#include "bsp_sdcard.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shell.h"

static const char *TAG = "test_shell";

void app_main(void)
{
    bsp_sdcard_handle_t sdcard = NULL;

    esp_err_t ret = bsp_sdcard_open(&sdcard);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_sdcard_open failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = bsp_sdcard_mount(sdcard, "/sdcard");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_sdcard_mount failed: %s", esp_err_to_name(ret));
        (void)bsp_sdcard_close(sdcard);
        return;
    }

    ret = shell_mount_add("/sdcard");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "shell_mount_add failed: %s", esp_err_to_name(ret));
        (void)bsp_sdcard_unmount(sdcard);
        (void)bsp_sdcard_close(sdcard);
        return;
    }

    shell_cfg_t shell_cfg = shell_default_config();
    shell_cfg.initial_path = "/sdcard";
    ret = shell_init(&shell_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "shell_init failed: %s", esp_err_to_name(ret));
        (void)shell_mount_remove("/sdcard");
        (void)bsp_sdcard_unmount(sdcard);
        (void)bsp_sdcard_close(sdcard);
        return;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

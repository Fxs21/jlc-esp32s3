#include "HAL.h"

#include "bsp_ui.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

namespace {

struct HalUiState {
    bsp_ui_handle_t ui = nullptr;
    bool inited = false;
};

struct HalState {
    bool inited = false;
    HalUiState ui;
};

HalState sHal;
constexpr char TAG[] = "HAL_CORE";

}  // namespace

static bool UI_Init()
{
    HalUiState &ui = sHal.ui;
    if (ui.inited) {
        return true;
    }

    esp_err_t err = ESP_OK;

    if (ui.ui == nullptr) {
        err = bsp_ui_open(&ui.ui);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "bsp_ui_open failed: %s", esp_err_to_name(err));
            return false;
        }
    }

    err = bsp_ui_fill(ui.ui, 0x0000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_ui_fill failed: %s", esp_err_to_name(err));
        return false;
    }

    err = bsp_ui_set_backlight(ui.ui, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_ui_set_backlight failed: %s", esp_err_to_name(err));
        return false;
    }

    ui.inited = true;
    return true;
}

bool HAL::HAL_Init()
{
    if (sHal.inited) {
        return true;
    }

    if (!UI_Init()) {
        ESP_LOGE(TAG, "HAL init aborted because UI init failed");
        return false;
    }

    Clock_Init();
    Power_Init();
    const bool sd_ready = SD_Init();
    GPS_Init();
    const bool imu_ready = IMU_Init();
    const bool mag_ready = MAG_Init();
    Encoder_Init();
    Buzz_Init();
    Audio_Init();

    sHal.inited = true;
    ESP_LOGI(TAG, "HAL initialized (sd=%s, imu=%s, mag=%s)",
             sd_ready ? "ok" : "fail",
             imu_ready ? "ok" : "fail",
             mag_ready ? "ok" : "fail");
    return true;
}

void HAL::HAL_Update()
{
    if (!sHal.inited) {
        return;
    }

    uint32_t lvgl_delay_ms = 0;
    (void)bsp_ui_process(sHal.ui.ui, &lvgl_delay_ms);
    GPS_Update();
    IMU_Update();
}

void HAL::Memory_DumpInfo()
{
    ESP_LOGI(TAG, "heap_free=%lu", (unsigned long)esp_get_free_heap_size());
}

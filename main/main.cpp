#include "App.h"
#include "Common/HAL/HAL.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    static constexpr char TAG[] = "main_task";
    if (!HAL::HAL_Init()) {
        ESP_LOGE(TAG, "HAL init failed, app init skipped");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    App_Init();

    while (true) {
        HAL::HAL_Update();
        // vTaskDelay(pdMS_TO_TICKS(10));
    }
}

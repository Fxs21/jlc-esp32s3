#include "bsp_ui.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_demos.h"
#include "lvgl.h"

#define TEST_BACKLIGHT_DELAY_MS 50

void app_main(void)
{
    bsp_ui_handle_t ui = NULL;
    ESP_ERROR_CHECK(bsp_ui_open(&ui));

    ESP_ERROR_CHECK(bsp_ui_fill(ui, 0x0000));
    vTaskDelay(pdMS_TO_TICKS(TEST_BACKLIGHT_DELAY_MS));
    ESP_ERROR_CHECK(bsp_ui_set_backlight(ui, 100));

    lv_demo_widgets();

    while (1) {
        uint32_t delay_ms = 0;
        ESP_ERROR_CHECK(bsp_ui_process(ui, &delay_ms));
#ifdef LV_NO_TIMER_READY
        if (delay_ms == LV_NO_TIMER_READY) {
            delay_ms = 5;
        }
#endif
        if (delay_ms == 0) {
            delay_ms = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

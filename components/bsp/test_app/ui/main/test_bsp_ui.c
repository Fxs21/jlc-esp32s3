#include "bsp_board.h"
#include "bsp_ui.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_demos.h"
#include "lvgl.h"

#define TAG "test_bsp_ui"

void app_main(void)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    ESP_LOGI(TAG, "TEST START name=ui board=%s", board ? board->name : "unknown");

    bsp_ui_handle_t ui = NULL;
    ESP_ERROR_CHECK(bsp_ui_open(&ui));

    // lv_demo_widgets();
    lv_demo_benchmark();
    // lv_demo_stress();

    ESP_ERROR_CHECK(bsp_ui_set_backlight(ui, 50));

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

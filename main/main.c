/**
 * @file main.c
 * @brief X-TRACK App on the jlc-esp32s3 BSP
 *
 * The App layer comes from the `xtrack_app` component (see main/idf_component.yml).
 * The board provides the HAL implementation (xtrack_hal_board) and the display
 * through the BSP public API.  App code never touches board pins, buses or
 * chip drivers.
 */
#include "bsp_board.h"
#include "bsp_ui.h"

#include "app.h"
#include "hal/hal.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "xtrack"

void app_main(void)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    ESP_LOGI(TAG, "X-TRACK app starting, board=%s", board ? board->name : "unknown");

    bsp_ui_handle_t ui = NULL;
    ESP_ERROR_CHECK(bsp_ui_open(&ui));
    ESP_ERROR_CHECK(bsp_ui_set_backlight(ui, 50));

    hal_init();
    app_init();

    while (1) {
        uint32_t delay_ms = 0;
        ESP_ERROR_CHECK(bsp_ui_process(ui, &delay_ms));
        if (delay_ms == 0)
            delay_ms = 1;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

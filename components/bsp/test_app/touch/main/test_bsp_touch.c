#include <stdio.h>

#include "bsp_touch.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "test_bsp_touch"
#define READ_COUNT 300

void app_main(void)
{
    if (!bsp_touch_get_desc()->present) {
        ESP_LOGW(TAG, "touch not present on this board");
        return;
    }

    bsp_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(bsp_touch_open(&touch));

    for (int i = 0; i < READ_COUNT; ++i) {
        esp_lcd_touch_point_data_t points[1] = {0};
        size_t points_num = 0;
        esp_err_t ret = bsp_touch_read(touch, points, 1, &points_num);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(ret));
            break;
        }
        if (points_num > 0) {
            printf("touch: x=%u y=%u strength=%u\n", points[0].x, points[0].y, points[0].strength);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_ERROR_CHECK(bsp_touch_close(touch));
}

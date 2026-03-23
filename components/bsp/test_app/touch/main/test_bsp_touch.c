#include <stdlib.h>

#include "bsp_board.h"
#include "bsp_touch.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "test_bsp_touch"
#define READ_COUNT 600
#define READ_INTERVAL_MS 50

void app_main(void)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    ESP_LOGI(TAG, "TEST START name=touch board=%s", board ? board->name : "unknown");

    const bsp_touch_desc_t *desc = bsp_touch_get_desc();
    if (!desc->present) {
        ESP_LOGW(TAG, "TEST SKIP name=touch reason=\"not present\"");
        return;
    }
    ESP_LOGI(TAG, "touch desc: max_points=%u x_max=%u y_max=%u",
             (unsigned)desc->max_points, (unsigned)desc->x_max, (unsigned)desc->y_max);

    bsp_touch_point_t *points = calloc(desc->max_points, sizeof(*points));
    if (points == NULL) {
        ESP_LOGE(TAG, "TEST FAIL name=touch step=alloc");
        return;
    }

    bsp_touch_handle_t touch = NULL;
    esp_err_t ret = bsp_touch_open(&touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=touch step=open err=%s", esp_err_to_name(ret));
        free(points);
        return;
    }

    int touches = 0;
    for (int i = 0; i < READ_COUNT; ++i) {
        size_t points_num = 0;
        ret = bsp_touch_read(touch, points, desc->max_points, &points_num);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "TEST FAIL name=touch step=read sample=%d err=%s", i, esp_err_to_name(ret));
            goto out;
        }

        if (points_num > 0) {
            touches++;
            ESP_LOGI(TAG, "sample=%d points=%u x=%u y=%u pressure=%u id=%u",
                     i,
                     (unsigned)points_num,
                     points[0].x,
                     points[0].y,
                     points[0].pressure,
                     points[0].id);
        }
        vTaskDelay(pdMS_TO_TICKS(READ_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "TEST PASS name=touch summary=\"samples=%d touches=%d\"", READ_COUNT, touches);

out:
    ret = bsp_touch_close(touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=touch step=close err=%s", esp_err_to_name(ret));
    }
    free(points);
}

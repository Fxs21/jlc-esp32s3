#include <inttypes.h>
#include <stdio.h>

#include "bsp_imu.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "test_bsp_imu"
#define READ_COUNT 100

void app_main(void)
{
    if (!bsp_imu_get_desc()->present) {
        ESP_LOGW(TAG, "imu not present on this board");
        return;
    }

    bsp_imu_handle_t imu = NULL;
    ESP_ERROR_CHECK(bsp_imu_open(&imu));

    for (int i = 0; i < READ_COUNT; ++i) {
        bool ready = false;
        esp_err_t ret = bsp_imu_is_data_ready(imu, &ready);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "data ready failed: %s", esp_err_to_name(ret));
            break;
        }
        if (!ready) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        bsp_imu_data_t data = {0};
        ret = bsp_imu_read(imu, &data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(ret));
            break;
        }

        printf("accel m/s2: x=% .3f y=% .3f z=% .3f\n", data.accel_x, data.accel_y, data.accel_z);
        printf("gyro  rad/s: x=% .3f y=% .3f z=% .3f\n", data.gyro_x, data.gyro_y, data.gyro_z);
        printf("temp      C: %.2f timestamp=%" PRIu32 "\n", data.temperature, data.timestamp);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_ERROR_CHECK(bsp_imu_close(imu));
}

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
    ESP_LOGI(TAG, "TEST START name=imu");
    if (!bsp_imu_get_desc()->present) {
        ESP_LOGW(TAG, "TEST SKIP name=imu reason=\"not present\"");
        return;
    }

    bsp_imu_handle_t imu = NULL;
    esp_err_t ret = bsp_imu_open(&imu);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=imu step=open err=%s", esp_err_to_name(ret));
        return;
    }

    int samples = 0;
    for (int i = 0; i < READ_COUNT; ++i) {
        bool ready = false;
        ret = bsp_imu_is_data_ready(imu, &ready);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "TEST FAIL name=imu step=data_ready err=%s", esp_err_to_name(ret));
            goto out;
        }
        if (!ready) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        bsp_imu_data_t data = {0};
        ret = bsp_imu_read(imu, &data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "TEST FAIL name=imu step=read err=%s", esp_err_to_name(ret));
            goto out;
        }
        samples++;

        ESP_LOGI(TAG, "accel m/s2: x=% .3f y=% .3f z=% .3f", data.accel_mps2_x, data.accel_mps2_y, data.accel_mps2_z);
        ESP_LOGI(TAG, "gyro  rad/s: x=% .3f y=% .3f z=% .3f", data.gyro_rads_x, data.gyro_rads_y, data.gyro_rads_z);
        ESP_LOGI(TAG, "temp      C: %.2f timestamp_ms=%" PRIu32, data.temperature_c, data.timestamp_ms);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (samples == 0) {
        ESP_LOGE(TAG, "TEST FAIL name=imu step=read summary=\"no samples\"");
    } else {
        ESP_LOGI(TAG, "TEST PASS name=imu summary=\"samples=%d\"", samples);
    }

out:
    ret = bsp_imu_close(imu);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=imu step=close err=%s", esp_err_to_name(ret));
    }
}

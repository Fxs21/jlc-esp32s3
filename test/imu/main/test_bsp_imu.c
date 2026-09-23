// test/imu: bsp_imu 板级自检. 只使用 BSP public API; 阈值是本文件的常量.

#include <math.h>
#include <stdint.h>

#include "bsp_imu.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "imu"

static const char *TAG = "test_bsp_imu";

// 静止平放时合加速度应接近 1g; 桌面不一定水平且有噪声, 留 ±15%.
#define ACCEL_MIN_MPS2 8.5f
#define ACCEL_MAX_MPS2 11.5f
// 静止时角速度应接近 0.
#define GYRO_MAX_RADS 1.0f
#define TEMP_MIN_C 0.0f
#define TEMP_MAX_C 60.0f
// 连续采样次数.
#define SAMPLE_COUNT 10
// is_data_ready 轮询上限; ODR 1000 Hz, 100 ms 内约 100 个样本.
#define READY_POLL_MS 100

static bsp_imu_handle_t s_imu;

static float vector_length(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

void setUp(void)
{
}

void tearDown(void)
{
    if (s_imu != NULL) {
        (void)bsp_imu_close(s_imu);
        s_imu = NULL;
    }
}

static void open_imu(void)
{
    s_imu = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_imu_open(&s_imu));
    TEST_ASSERT_NOT_NULL(s_imu);
}

static void read_sample(bsp_imu_data_t *data)
{
    *data = (bsp_imu_data_t){0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_imu_read(s_imu, data));
}

TEST_CASE("imu: open and close", "[imu]")
{
    open_imu();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_imu_close(s_imu));
    s_imu = NULL;
}

TEST_CASE("imu: second open rejected until closed", "[imu]")
{
    open_imu();

    bsp_imu_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_imu_open(&second));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_imu_close(s_imu));
    s_imu = NULL;

    open_imu();
}

TEST_CASE("imu: null arguments rejected", "[imu]")
{
    bsp_imu_data_t data = {0};
    bool ready = false;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_imu_read(NULL, &data));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_imu_is_data_ready(NULL, &ready));

    open_imu();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_imu_read(s_imu, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_imu_is_data_ready(s_imu, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_imu_close(NULL));
}

TEST_CASE("imu: readings are in range", "[imu]")
{
    open_imu();

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        bsp_imu_data_t data;
        read_sample(&data);

        const float accel = vector_length(data.accel_mps2_x, data.accel_mps2_y, data.accel_mps2_z);
        const float gyro = vector_length(data.gyro_rads_x, data.gyro_rads_y, data.gyro_rads_z);

        ESP_LOGI(TAG, "sample %2d: |a|=%6.2f m/s2  |w|=%6.3f rad/s  temp=%5.2f C",
                 i, accel, gyro, data.temperature_c);

        TEST_ASSERT_TRUE(accel >= ACCEL_MIN_MPS2 && accel <= ACCEL_MAX_MPS2);
        TEST_ASSERT_TRUE(gyro <= GYRO_MAX_RADS);
        TEST_ASSERT_TRUE(data.temperature_c >= TEMP_MIN_C && data.temperature_c <= TEMP_MAX_C);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

TEST_CASE("imu: sample counter advances", "[imu]")
{
    open_imu();

    uint32_t previous = 0;
    int advances = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        bsp_imu_data_t data;
        read_sample(&data);

        if (i > 0) {
            TEST_ASSERT_TRUE(data.timestamp_ticks > previous);
            advances++;
        }
        previous = data.timestamp_ticks;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ESP_LOGI(TAG, "sample counter advanced %d times over %d samples", advances, SAMPLE_COUNT);
}

TEST_CASE("imu: data ready reported within 100 ms", "[imu]")
{
    open_imu();

    bool saw_ready = false;
    int waited_ms = 0;
    while (waited_ms < READY_POLL_MS) {
        bool ready = false;
        TEST_ASSERT_EQUAL(ESP_OK, bsp_imu_is_data_ready(s_imu, &ready));
        if (ready) {
            saw_ready = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        waited_ms += 5;
    }
    ESP_LOGI(TAG, "data ready observed=%d after %d ms", (int)saw_ready, waited_ms);
    TEST_ASSERT_TRUE(saw_ready);
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_imu_desc_t *desc = bsp_imu_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no imu on this board");
        return;
    }

    selftest_run(MODULE);
}

#include "HAL.h"

#include <math.h>

#include "bsp_imu.h"
#include "esp_err.h"
#include "esp_log.h"

namespace {

struct ImuState {
    bsp_imu_handle_t handle = nullptr;
    HAL::CommitFunc_t commit = nullptr;
    void *commit_user = nullptr;
};

ImuState sImu;
constexpr char TAG[] = "HAL_IMU";

}  // namespace

bool HAL::IMU_Init()
{
    if (sImu.handle != nullptr) {
        return true;
    }

    esp_err_t err = bsp_imu_new(&sImu.handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_imu_new failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

void HAL::IMU_SetCommitCallback(CommitFunc_t func, void *userData)
{
    sImu.commit = func;
    sImu.commit_user = userData;
}

void HAL::IMU_Update()
{
    if (sImu.handle == nullptr || sImu.commit == nullptr) {
        return;
    }

    bsp_imu_data_t imu_data = {};
    if (bsp_imu_read(sImu.handle, &imu_data) != ESP_OK) {
        return;
    }

    IMU_Info_t imu = {};
    imu.ax = (int16_t)lrintf(imu_data.accel_x * 100.0f);
    imu.ay = (int16_t)lrintf(imu_data.accel_y * 100.0f);
    imu.az = (int16_t)lrintf(imu_data.accel_z * 100.0f);
    imu.gx = (int16_t)lrintf(imu_data.gyro_x * 10.0f);
    imu.gy = (int16_t)lrintf(imu_data.gyro_y * 10.0f);
    imu.gz = (int16_t)lrintf(imu_data.gyro_z * 10.0f);
    imu.steps = 0;

    (void)sImu.commit(&imu, sImu.commit_user);
}

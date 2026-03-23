#include "bsp_imu_backend.h"
#include "bsp_imu_qmi8658.h"

static const bsp_imu_qmi8658_cfg_t doers3_imu_cfg = {
    .address = 0x6A,
    .accel_unit_mps2 = true,
    .gyro_unit_rads = true,
    .display_precision = 4,
};

const bsp_imu_backend_t bsp_imu_backend_doers3 = {
    .config = &doers3_imu_cfg,
    .create = bsp_imu_qmi8658_create,
    .destroy = bsp_imu_qmi8658_destroy,
    .read = bsp_imu_qmi8658_read,
    .is_data_ready = bsp_imu_qmi8658_is_data_ready,
};

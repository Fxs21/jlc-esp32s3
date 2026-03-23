#include "bsp_imu_backend.h"
#include "bsp_boarddb.h"
#include "bsp_imu_qmi8658.h"

const bsp_imu_backend_t *bsp_imu_backend_get(void)
{
    static bsp_imu_qmi8658_cfg_t cfg;
    static bsp_imu_backend_t backend;
    static bool initialized;

    const bsp_boarddb_imu_desc_t *imu = &bsp_boarddb_get()->imu;
    if (!imu->present) {
        return NULL;
    }

    if (!initialized) {
        cfg.address = imu->address;
        cfg.accel_unit_mps2 = imu->accel_unit_mps2;
        cfg.gyro_unit_rads = imu->gyro_unit_rads;
        cfg.display_precision = imu->display_precision;

        backend.config = &cfg;
        backend.create = bsp_imu_qmi8658_create;
        backend.destroy = bsp_imu_qmi8658_destroy;
        backend.read = bsp_imu_qmi8658_read;
        backend.is_data_ready = bsp_imu_qmi8658_is_data_ready;
        initialized = true;
    }

    return &backend;
}

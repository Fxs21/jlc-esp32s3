idf_component_register(
    SRCS
        "src/bsp_imu.c"
        "src/bsp_imu_backend_select.c"
        "src/drivers/bsp_imu_qmi8658.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src" "src/drivers"
    REQUIRES driver waveshare__qmi8658 bsp_boarddb
    PRIV_REQUIRES bsp_i2c_bus
)

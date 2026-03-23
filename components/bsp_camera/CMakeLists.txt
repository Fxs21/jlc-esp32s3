idf_component_register(
    SRCS
        "src/bsp_camera.c"
        "src/bsp_camera_backend_select.c"
        "src/drivers/bsp_camera_gc0308.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src" "src/drivers"
    REQUIRES driver esp32-camera bsp_boarddb
    PRIV_REQUIRES bsp_i2c_bus bsp_ioexp
)

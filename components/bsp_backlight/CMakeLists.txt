idf_component_register(
    SRCS
        "src/bsp_backlight.c"
        "src/bsp_backlight_backend_select.c"
        "src/drivers/bsp_backlight_ledc.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src/drivers"
    REQUIRES driver bsp_boarddb
)

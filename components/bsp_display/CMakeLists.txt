idf_component_register(
    SRCS
        "src/bsp_display.c"
        "src/bsp_display_backend_select.c"
        "src/drivers/bsp_display_st7789.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src/drivers"
    REQUIRES driver esp_lcd bsp_boarddb
    PRIV_REQUIRES bsp_ioexp
)

idf_component_register(
    SRCS
        "src/bsp_touch.c"
        "src/bsp_touch_backend_select.c"
        "src/drivers/bsp_touch_ft5x06.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src/drivers"
    REQUIRES esp_lcd espressif__esp_lcd_touch_ft5x06 bsp_boarddb
    PRIV_REQUIRES bsp_i2c_bus
)

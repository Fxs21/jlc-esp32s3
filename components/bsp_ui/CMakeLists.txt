idf_component_register(
    SRCS "src/bsp_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_display bsp_touch bsp_backlight lvgl
    PRIV_REQUIRES esp_timer esp_lcd
)

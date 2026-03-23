idf_component_register(SRCS "test_bsp_camera.c"
                       PRIV_REQUIRES bsp_backlight bsp_camera bsp_display esp_timer
                       INCLUDE_DIRS "")

# Keep board-selection Kconfig in this component so minimal test apps that only
# depend on boarddb still get the board choice.
idf_component_register(
    SRCS
        "src/bsp_boarddb.c"
        "src/boards/auras3.c"
        "src/boards/doers3.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES driver esp_lcd esp32-camera
)

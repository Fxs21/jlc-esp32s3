idf_component_register(
    SRCS "src/bsp_sdcard.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs driver sdmmc bsp_boarddb
)

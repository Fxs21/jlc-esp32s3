idf_component_register(
    SRCS "src/bsp_gnss.c"
    INCLUDE_DIRS "include"
    REQUIRES driver bsp_boarddb
)

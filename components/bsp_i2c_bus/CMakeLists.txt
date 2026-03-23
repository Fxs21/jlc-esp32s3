idf_component_register(
    SRCS "src/bsp_i2c_bus.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos
    PRIV_REQUIRES bsp_boarddb
)

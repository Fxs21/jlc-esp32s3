idf_component_register(
    SRCS
        "src/bsp_ioexp.c"
        "src/drivers/bsp_ioexp_pca9557.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src/drivers"
    REQUIRES driver freertos
    PRIV_REQUIRES bsp_boarddb bsp_i2c_bus
)

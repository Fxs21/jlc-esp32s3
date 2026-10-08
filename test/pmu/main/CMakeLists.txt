idf_component_register(
    SRCS "test_bsp_pmu.c"
    PRIV_REQUIRES selftest bsp esp_timer
    INCLUDE_DIRS ""
)

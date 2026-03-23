idf_component_register(
    SRCS
        "src/bsp_audio.c"
        "src/drivers/bsp_audio_es8311_es7210.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src/drivers"
    REQUIRES esp_codec_dev driver bsp_boarddb
    PRIV_REQUIRES bsp_i2c_bus bsp_ioexp
)

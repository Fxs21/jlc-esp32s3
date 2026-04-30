set(srcs
    "src/common/ui.c"
    "src/common/camera_stub.c"
)

set(priv_include_dirs
    "src"
    "src/drivers/display"
    "src/drivers/touch"
    "src/drivers/backlight"
)

if(CONFIG_BSP_BOARD_DOERS3)
    list(APPEND srcs
        "src/boards/doers3/board.c"
        "src/boards/doers3/display.c"
        "src/boards/doers3/touch.c"
        "src/boards/doers3/backlight.c"
        "src/boards/doers3/sdcard.c"
        "src/boards/doers3/gnss.c"
        "src/boards/doers3/imu.c"
        "src/boards/doers3/audio.c"
        "src/boards/doers3/internal/doers3_i2c.c"
        "src/boards/doers3/internal/doers3_ioexp.c"
        "src/boards/doers3/internal/pca9557.c"
        "src/drivers/display/st7789.c"
        "src/drivers/touch/ft5x06.c"
        "src/drivers/backlight/ledc.c"
    )
    list(APPEND priv_include_dirs "src/boards/doers3/internal")
elseif(CONFIG_BSP_BOARD_AURAS3)
    list(APPEND srcs
        "src/boards/auras3/board.c"
        "src/boards/auras3/display.c"
        "src/boards/auras3/touch.c"
        "src/boards/auras3/backlight.c"
        "src/boards/auras3/sdcard.c"
        "src/boards/auras3/gnss.c"
        "src/common/audio_stub.c"
        "src/common/imu_stub.c"
    )
    list(APPEND priv_include_dirs "src/boards/auras3/internal")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    REQUIRES
        driver
        esp_lcd
        espressif__esp_lcd_touch_ft5x06
        fatfs
        sdmmc
        lvgl
        esp32-camera
        waveshare__qmi8658
        espressif__esp_codec_dev
    PRIV_REQUIRES
        esp_timer
        freertos
)

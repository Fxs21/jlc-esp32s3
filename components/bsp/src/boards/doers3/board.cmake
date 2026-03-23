set(BSP_BOARD_SRCS
    "src/boards/doers3/board.c"
    "src/boards/doers3/display.c"
    "src/boards/doers3/touch.c"
    "src/boards/doers3/backlight.c"
    "src/boards/doers3/sdcard.c"
    "src/boards/doers3/gnss.c"
    "src/boards/doers3/imu.c"
    "src/boards/doers3/audio.c"
    "src/common/unsupported/pmu_unsupported.c"
    "src/boards/doers3/internal/doers3_i2c.c"
    "src/boards/doers3/internal/doers3_ioexp.c"
)

if(CONFIG_BSP_ENABLE_CAMERA)
    list(APPEND BSP_BOARD_SRCS "src/boards/doers3/camera.c")
endif()

set(BSP_BOARD_DRIVERS
    "src/drivers/display/st7789.c"
    "src/drivers/touch/ft6336.c"
    "src/drivers/backlight/ledc.c"
    "src/drivers/audio/es8311.c"
    "src/drivers/audio/es7210.c"
    "src/drivers/imu/qmi8658.c"
    "src/drivers/ioexp/pca9557.c"
)

set(BSP_BOARD_PRIV_INCLUDE_DIRS
    "src/boards/doers3/internal"
)

if(CONFIG_BSP_ENABLE_CAMERA)
    set(BSP_BOARD_PRIV_LINK_LIBS
        idf::espressif__esp32-camera
        idf::espressif__esp_jpeg
    )
endif()

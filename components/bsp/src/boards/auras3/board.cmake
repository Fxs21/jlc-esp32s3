set(BSP_BOARD_SRCS
    "src/boards/auras3/board.c"
    "src/boards/auras3/display.c"
    "src/boards/auras3/touch.c"
    "src/boards/auras3/backlight.c"
    "src/boards/auras3/imu.c"
    "src/boards/auras3/audio.c"
    "src/boards/auras3/gnss.c"
    "src/boards/auras3/sdcard.c"
    "src/boards/auras3/pmu.c"
    "src/common/unsupported/camera_unsupported.c"
    "src/boards/auras3/internal/auras3_panel.c"
    "src/boards/auras3/internal/auras3_i2c.c"
    "src/boards/auras3/internal/auras3_ioexp.c"
)

set(BSP_BOARD_DRIVERS
    "src/drivers/touch/cst9217.c"
    "src/drivers/audio/es8311.c"
    "src/drivers/audio/es7210.c"
    "src/drivers/imu/qmi8658.c"
    "src/drivers/ioexp/tca9554.c"
    "src/drivers/power/axp2101.c"
)

set(BSP_BOARD_PRIV_INCLUDE_DIRS
    "src/boards/auras3/internal"
    "src/drivers/power"
)

set(BSP_BOARD_PRIV_LINK_LIBS
    idf::espressif__esp_lcd_co5300
)

set(srcs
    "src/common/ui.c"
    "src/common/sdcard.c"
    "src/common/audio.c"
    "src/common/ioexp.c"
    "src/common/bus/bsp_i2c_bus.c"
    "src/common/bus/bsp_i2c_scan.c"
    "src/common/lvgl/bsp_lvgl_buffer.c"
    "src/common/lvgl/bsp_lvgl_port.c"
)

set(priv_include_dirs
    "src"
    "src/internal"
    "src/common"
    "src/common/bus"
    "src/common/lvgl"
    "src/drivers/display"
    "src/drivers/touch"
    "src/drivers/backlight"
    "src/drivers/audio"
    "src/drivers/imu"
    "src/drivers/ioexp"
)

set(bsp_priv_requires
    driver
    esp_lcd
    esp_timer
    fatfs
    freertos
    sdmmc
)

set(BSP_BOARD_SRCS)
set(BSP_BOARD_DRIVERS)
set(BSP_BOARD_PRIV_INCLUDE_DIRS)
set(BSP_BOARD_PRIV_LINK_LIBS)

if(CONFIG_BSP_BOARD_DOERS3)
    include("${CMAKE_CURRENT_LIST_DIR}/src/boards/doers3/board.cmake")
elseif(CONFIG_BSP_BOARD_AURAS3)
    include("${CMAKE_CURRENT_LIST_DIR}/src/boards/auras3/board.cmake")
endif()

list(APPEND srcs ${BSP_BOARD_SRCS} ${BSP_BOARD_DRIVERS})
list(APPEND priv_include_dirs ${BSP_BOARD_PRIV_INCLUDE_DIRS})

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    REQUIRES
        esp_driver_i2c
        lvgl__lvgl
    PRIV_REQUIRES
        ${bsp_priv_requires}
)

# Board-specific dependencies are linked after registration because ESP-IDF
# expands component requirements before Kconfig board choices are final.
if(BSP_BOARD_PRIV_LINK_LIBS)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${BSP_BOARD_PRIV_LINK_LIBS})
endif()

# Test apps keep managed-component dependencies explicit.
if(TARGET __idf_espressif__esp32-camera AND TARGET idf::espressif__esp_jpeg)
    target_link_libraries(__idf_espressif__esp32-camera PUBLIC idf::espressif__esp_jpeg)
endif()

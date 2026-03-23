file(GLOB_RECURSE XTRACK_APP_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/App/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/App/*.cpp"
)

set(XTRACK_HAL_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_Audio.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_Buzz.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_Clock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_Encoder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_GPS.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_IMU.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_MAG.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_Power.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/HAL_SD_CARD.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HAL/TinyGPSPlus/TinyGPS++.cpp"
)

# Keep upstream App includes unchanged: provide lvgl/lvgl.h compatibility
# from build directory and map it to official <lvgl.h>.
set(XTRACK_LVGL_COMPAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/xtrack_lvgl_compat")
file(MAKE_DIRECTORY "${XTRACK_LVGL_COMPAT_DIR}/lvgl")
file(WRITE "${XTRACK_LVGL_COMPAT_DIR}/lvgl/lvgl.h" "#pragma once\n#include <lvgl.h>\n")

idf_component_register(
    SRCS
        ${XTRACK_APP_SOURCES}
        ${XTRACK_HAL_SOURCES}
    INCLUDE_DIRS
        "App"
        "App/Common"
        "App/Config"
        "App/Pages"
        "App/Resource"
        "App/Utils"
        "App/Utils/cJSON"
        "HAL"
        "${XTRACK_LVGL_COMPAT_DIR}"
    REQUIRES
        bsp_gnss
        bsp_imu
        bsp_ui
        bsp_sdcard
        esp_timer
        lvgl
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)

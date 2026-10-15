cmake_minimum_required(VERSION 3.20)
project(lvgl-simulator C)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# LVGL submodule at sim/third_party/lvgl; add_subdirectory creates the `lvgl`
# target. lv_conf.h is auto-detected from CMAKE_SOURCE_DIR (this dir), or set
# LV_BUILD_CONF_PATH for a custom location.
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build demos")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")

add_subdirectory(third_party/lvgl)

# xtrack App layer source list (shared with the IDF component)
include(${CMAKE_CURRENT_LIST_DIR}/../xtrack/app_sources.cmake)

# PC backend: entry point + SDL display/input + simulated HAL implementations
set(SIM_SRCS
    main.c
    hal/hal_clock.c
    hal/hal_gps.c
    hal/hal_power.c
    hal/hal_sdl.c
)

add_executable(lvgl-sim ${SIM_SRCS} ${XTRACK_APP_SRCS})

target_include_directories(lvgl-sim PRIVATE
    ${XTRACK_APP_INCLUDE_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/hal
)

target_link_libraries(lvgl-sim PRIVATE lvgl SDL2 m)

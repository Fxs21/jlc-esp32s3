cmake_minimum_required(VERSION 3.20)
project(lvgl-simulator C)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# LVGL — add_subdirectory creates `lvgl` library target
# lv_conf.h is auto-detected from CMAKE_SOURCE_DIR (this dir)
# or set LV_BUILD_CONF_PATH for custom location
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build demos")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")

add_subdirectory(third_party/lvgl)

# Our application
set(CORE_SOURCES
    main.c
    app.c
)

set(HAL_SOURCES
    hal/hal_clock.c
    hal/hal_gps.c
    hal/hal_init.c
    hal/hal_power.c
    hal/hal_sdl.c
)

set(DATA_CENTER_SOURCES
    data_center/account.c
    data_center/data_center.c
)

set(DATA_PROC_SOURCES
    data_proc/data_proc.c
    data_proc/dp_clock.c
    data_proc/dp_gps.c
    data_proc/dp_power.c
)

set(PAGE_MANAGER_SOURCES
    page_manager/page.c
    page_manager/page_manager.c
    page_manager/pm_anim.c
    page_manager/pm_drag.c
    page_manager/pm_state.c
)

set(PAGES_SOURCES
    pages/dialplate/dialplate_model.c
    pages/dialplate/dialplate_page.c
    pages/dialplate/dialplate_view.c
    pages/livemap/livemap_model.c
    pages/livemap/livemap_page.c
    pages/livemap/livemap_view.c
    pages/startup/startup_model.c
    pages/startup/startup_page.c
    pages/startup/startup_view.c
    pages/status_bar/status_bar.c
    pages/system_infos/system_infos_model.c
    pages/system_infos/system_infos_page.c
    pages/system_infos/system_infos_view.c
)

file(GLOB RESOURCE_SOURCES resource/image/*.c)
set(RESOURCE_POOL resource/resource_pool.c)

set(DS_SOURCES
    utils/ds/vector.c
)

set(SOURCES
    ${CORE_SOURCES}
    ${HAL_SOURCES}
    ${DATA_CENTER_SOURCES}
    ${DATA_PROC_SOURCES}
    ${PAGE_MANAGER_SOURCES}
    ${PAGES_SOURCES}
    ${RESOURCE_SOURCES}
    ${RESOURCE_POOL}
    ${DS_SOURCES}
)

add_executable(lvgl-sim ${SOURCES})

# lvgl already includes lv_conf.h path via its own target
# We just need SDL2, our own headers, and third_party (uthash)
target_include_directories(lvgl-sim PRIVATE
    third_party
    hal
    data_center
    data_proc
    page_manager
    pages/dialplate
    pages/livemap
    pages/startup
    pages/status_bar
    pages/system_infos
    resource
    resource/image
    utils
    utils/ds
)
target_link_libraries(lvgl-sim PRIVATE lvgl SDL2 m)

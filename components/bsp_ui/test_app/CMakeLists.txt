# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET esp32s3 CACHE STRING "ESP-IDF target" FORCE)
endif()

set(EXTRA_COMPONENT_DIRS
    "${REPO_ROOT}/components/bsp_backlight"
    "${REPO_ROOT}/components/bsp_boarddb"
    "${REPO_ROOT}/components/bsp_i2c_bus"
    "${REPO_ROOT}/components/bsp_display"
    "${REPO_ROOT}/components/bsp_ioexp"
    "${REPO_ROOT}/components/bsp_touch"
    "${REPO_ROOT}/components/bsp_ui"
    "${REPO_ROOT}/managed_components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(test_bsp_ui)

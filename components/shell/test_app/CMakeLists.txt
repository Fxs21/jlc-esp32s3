# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET esp32s3 CACHE STRING "ESP-IDF target")
endif()

set(EXTRA_COMPONENT_DIRS
    "${REPO_ROOT}/components/shell"
    "${REPO_ROOT}/components/bsp"
    "${REPO_ROOT}/managed_components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(test_shell)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET esp32s3 CACHE STRING "ESP-IDF target" FORCE)
endif()

set(EXTRA_COMPONENT_DIRS
    "${REPO_ROOT}/components/bsp"
    "${REPO_ROOT}/managed_components"
)

if(TEST_APP_USES_SHELL)
    list(PREPEND EXTRA_COMPONENT_DIRS "${REPO_ROOT}/components/shell")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_BINARY_DIR}/dependencies.lock")
idf_build_set_property(MINIMAL_BUILD ON)

# IDF component wrapper for the platform-neutral App layer of xtrack-app.
#
# Scope (the App layer only):
#   in : app.c, page_manager/, pages/, data_center/, data_proc/, resource/, utils/
#   out: main.c, hal/*.c (per-platform HAL implementations), lv_conf.h,
#        the repo-root CMakeLists.txt (PC build), third_party/lvgl (submodule)
#
# The HAL *interface* headers (hal/hal.h, hal/hal_clock.h, hal/hal_gps.h,
# hal/hal_power.h) belong to this component; the *implementation* of those
# modules is provided by the platform:
#   - PC simulator : hal/hal_clock.c, hal_gps.c, hal_power.c (this repo)
#   - board port   : components/xtrack_hal_board/ (in the board project)
#
# Nothing in here may reference SDL: the PC-only display/input init lives in
# hal/hal_sdl.h and is compiled by the PC build only.

set(XTRACK_APP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

set(XTRACK_APP_SRCS
    "${XTRACK_APP_ROOT}/app.c"

    "${XTRACK_APP_ROOT}/data_center/account.c"
    "${XTRACK_APP_ROOT}/data_center/data_center.c"

    "${XTRACK_APP_ROOT}/data_proc/data_proc.c"
    "${XTRACK_APP_ROOT}/data_proc/dp_clock.c"
    "${XTRACK_APP_ROOT}/data_proc/dp_gps.c"
    "${XTRACK_APP_ROOT}/data_proc/dp_power.c"

    "${XTRACK_APP_ROOT}/page_manager/page.c"
    "${XTRACK_APP_ROOT}/page_manager/page_manager.c"
    "${XTRACK_APP_ROOT}/page_manager/pm_anim.c"
    "${XTRACK_APP_ROOT}/page_manager/pm_drag.c"
    "${XTRACK_APP_ROOT}/page_manager/pm_state.c"

    "${XTRACK_APP_ROOT}/pages/dialplate/dialplate_model.c"
    "${XTRACK_APP_ROOT}/pages/dialplate/dialplate_page.c"
    "${XTRACK_APP_ROOT}/pages/dialplate/dialplate_view.c"
    "${XTRACK_APP_ROOT}/pages/livemap/livemap_model.c"
    "${XTRACK_APP_ROOT}/pages/livemap/livemap_page.c"
    "${XTRACK_APP_ROOT}/pages/livemap/livemap_view.c"
    "${XTRACK_APP_ROOT}/pages/startup/startup_model.c"
    "${XTRACK_APP_ROOT}/pages/startup/startup_page.c"
    "${XTRACK_APP_ROOT}/pages/startup/startup_view.c"
    "${XTRACK_APP_ROOT}/pages/status_bar/status_bar.c"
    "${XTRACK_APP_ROOT}/pages/system_infos/system_infos_model.c"
    "${XTRACK_APP_ROOT}/pages/system_infos/system_infos_page.c"
    "${XTRACK_APP_ROOT}/pages/system_infos/system_infos_view.c"

    "${XTRACK_APP_ROOT}/resource/resource_pool.c"

    "${XTRACK_APP_ROOT}/utils/ds/vector.c"
)

file(GLOB XTRACK_APP_IMAGE_SRCS "${XTRACK_APP_ROOT}/resource/image/*.c")

idf_component_register(
    SRCS
        ${XTRACK_APP_SRCS}
        ${XTRACK_APP_IMAGE_SRCS}
    INCLUDE_DIRS
        "${XTRACK_APP_ROOT}"
        "${XTRACK_APP_ROOT}/hal"
        "${XTRACK_APP_ROOT}/data_center"
        "${XTRACK_APP_ROOT}/data_proc"
        "${XTRACK_APP_ROOT}/page_manager"
        "${XTRACK_APP_ROOT}/pages/dialplate"
        "${XTRACK_APP_ROOT}/pages/livemap"
        "${XTRACK_APP_ROOT}/pages/startup"
        "${XTRACK_APP_ROOT}/pages/status_bar"
        "${XTRACK_APP_ROOT}/pages/system_infos"
        "${XTRACK_APP_ROOT}/resource"
        "${XTRACK_APP_ROOT}/resource/image"
        "${XTRACK_APP_ROOT}/utils"
        "${XTRACK_APP_ROOT}/utils/ds"
        # uthash (header-only, tracked under third_party/)
        "${XTRACK_APP_ROOT}/third_party"
    REQUIRES
        lvgl
)

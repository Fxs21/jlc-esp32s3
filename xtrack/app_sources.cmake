# xtrack App 层的源文件与 include 路径, 唯一出处.
#
# IDF 组件 (xtrack/CMakeLists.txt) 与 PC 模拟器 (sim/CMakeLists.txt) 都 include
# 本文件; 两边不允许再各自维护第二份清单.
#
# 路径基准: 本文件所在目录 (xtrack/).

set(XTRACK_APP_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(XTRACK_APP_SRCS
    "${XTRACK_APP_DIR}/app.c"
    "${XTRACK_APP_DIR}/hal/hal_init.c"

    "${XTRACK_APP_DIR}/data_center/account.c"
    "${XTRACK_APP_DIR}/data_center/data_center.c"

    "${XTRACK_APP_DIR}/data_proc/data_proc.c"
    "${XTRACK_APP_DIR}/data_proc/dp_clock.c"
    "${XTRACK_APP_DIR}/data_proc/dp_gps.c"
    "${XTRACK_APP_DIR}/data_proc/dp_power.c"

    "${XTRACK_APP_DIR}/page_manager/page.c"
    "${XTRACK_APP_DIR}/page_manager/page_manager.c"
    "${XTRACK_APP_DIR}/page_manager/pm_anim.c"
    "${XTRACK_APP_DIR}/page_manager/pm_drag.c"
    "${XTRACK_APP_DIR}/page_manager/pm_state.c"

    "${XTRACK_APP_DIR}/pages/dialplate/dialplate_model.c"
    "${XTRACK_APP_DIR}/pages/dialplate/dialplate_page.c"
    "${XTRACK_APP_DIR}/pages/dialplate/dialplate_view.c"
    "${XTRACK_APP_DIR}/pages/livemap/livemap_model.c"
    "${XTRACK_APP_DIR}/pages/livemap/livemap_page.c"
    "${XTRACK_APP_DIR}/pages/livemap/livemap_view.c"
    "${XTRACK_APP_DIR}/pages/startup/startup_model.c"
    "${XTRACK_APP_DIR}/pages/startup/startup_page.c"
    "${XTRACK_APP_DIR}/pages/startup/startup_view.c"
    "${XTRACK_APP_DIR}/pages/status_bar/status_bar.c"
    "${XTRACK_APP_DIR}/pages/system_infos/system_infos_model.c"
    "${XTRACK_APP_DIR}/pages/system_infos/system_infos_page.c"
    "${XTRACK_APP_DIR}/pages/system_infos/system_infos_view.c"

    "${XTRACK_APP_DIR}/resource/resource_pool.c"

    "${XTRACK_APP_DIR}/utils/ds/vector.c"
)

file(GLOB XTRACK_APP_IMAGE_SRCS "${XTRACK_APP_DIR}/resource/image/*.c")
list(APPEND XTRACK_APP_SRCS ${XTRACK_APP_IMAGE_SRCS})

set(XTRACK_APP_INCLUDE_DIRS
    "${XTRACK_APP_DIR}"
    "${XTRACK_APP_DIR}/hal"
    "${XTRACK_APP_DIR}/data_center"
    "${XTRACK_APP_DIR}/data_proc"
    "${XTRACK_APP_DIR}/page_manager"
    "${XTRACK_APP_DIR}/pages/dialplate"
    "${XTRACK_APP_DIR}/pages/livemap"
    "${XTRACK_APP_DIR}/pages/startup"
    "${XTRACK_APP_DIR}/pages/status_bar"
    "${XTRACK_APP_DIR}/pages/system_infos"
    "${XTRACK_APP_DIR}/resource"
    "${XTRACK_APP_DIR}/resource/image"
    "${XTRACK_APP_DIR}/utils"
    "${XTRACK_APP_DIR}/utils/ds"
    "${XTRACK_APP_DIR}/third_party"
)

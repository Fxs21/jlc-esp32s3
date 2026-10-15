# xtrack App 层的 IDF 组件包装.
#
# 源清单唯一出处: app_sources.cmake (sim 构建 include 同一份文件).
# 平台实现由使用方提供:
#   - PC 模拟器: sim/hal/*.c
#   - 板子:      main/ 里的板侧 HAL (待接入)
#
# 本组件平台无关: 不允许出现 SDL, IDF 或 BSP 头文件, 只依赖 LVGL 和 C 标准库.

include(${CMAKE_CURRENT_LIST_DIR}/app_sources.cmake)

idf_component_register(
    SRCS ${XTRACK_APP_SRCS}
    INCLUDE_DIRS ${XTRACK_APP_INCLUDE_DIRS}
    REQUIRES lvgl
)

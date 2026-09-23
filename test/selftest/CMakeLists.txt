# 板级自检 app 的公共骨架: 指纹行, 汇总行, SKIP 语义和人工确认输入.
# 只被 test/ 下的自检 app 使用, 不属于 BSP 交付面.

# 构建期注入 git 指纹, 让自检日志自己证明"跑的是哪个版本".
# 每次 bsp.sh 调用都会重新执行 configure, 因此提交后重建即可刷新.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
    OUTPUT_VARIABLE _selftest_sha
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT _selftest_sha)
    set(_selftest_sha "unknown")
endif()

# 工作区有未提交改动时打 -dirty 后缀: 此时日志不能单独作为验证证据.
execute_process(
    COMMAND git status --porcelain --untracked-files=no
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
    OUTPUT_VARIABLE _selftest_dirty
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(_selftest_dirty)
    set(_selftest_sha "${_selftest_sha}-dirty")
endif()

idf_component_register(
    SRCS "selftest.c"
    INCLUDE_DIRS "include"
    REQUIRES unity
    PRIV_REQUIRES bsp driver esp_timer
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC TEST_GIT_SHA="${_selftest_sha}")

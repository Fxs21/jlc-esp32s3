#pragma once

// 板级自检 app 的公共骨架: 指纹行, 汇总行, SKIP 语义和人工确认输入.
// 只被 test/ 下的自检 app 使用, 不属于 BSP 交付面.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 构建期注入的 git 指纹 (短 sha; 构建时工作区有未提交改动则带 -dirty 后缀).
extern const char selftest_git_sha[];

// 在其它输出之前调用一次, 打印指纹行.
//   SELFTEST start module=imu board=AuraS3 sha=1a2b3c4d
void selftest_start(const char *module);

// 跑完全部 unity 用例并打印汇总行, 返回是否全部通过.
//   SELFTEST end module=imu board=AuraS3 sha=1a2b3c4d tests=4 failed=0 result=PASS
bool selftest_run(const char *module);

// 模块在本板不存在时使用: 打印 SKIP 行, 不算失败.
//   SELFTEST end module=camera board=AuraS3 sha=1a2b3c4d result=SKIP reason=no camera
void selftest_skip(const char *module, const char *reason);

// 人工确认: 打印提示并从 console 读一行. 返回 1 = y, 0 = n, -1 = 超时或无输入.
int selftest_ask_yes_no(const char *prompt, int timeout_ms);

// 人工确认并把结论写成结构化行; 只有回答 y 才返回 true.
//   SELFTEST human module=display board=AuraS3 item=ghosting result=yes|no|pending
bool selftest_human_check(const char *module, const char *item, const char *prompt, int timeout_ms);

#ifdef __cplusplus
}
#endif

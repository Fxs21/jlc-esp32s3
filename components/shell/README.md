# shell 组件使用说明

`shell` 是一个基于 ESP-IDF `esp_console` 的轻量命令行组件。
它提供最小文件系统浏览能力，适合在串口或 USB 控制台做板级调试。

## 1. 当前能力范围

对外 API 位于 `components/shell/include/shell.h`：

- `shell_default_config()`
- `shell_init()` / `shell_deinit()`
- `shell_mount_add()` / `shell_mount_remove()`

当前内置命令：

- `pwd`：打印当前目录
- `cd [path]`：切换目录
- `ls [path]`：列出目录内容

## 2. 路径与挂载模型

- shell 的根目录固定为 `/`，它是一个虚拟根。
- 在 `/` 下执行 `ls` 时，显示的是已注册挂载点（如 `sdcard/`）。
- 真实文件系统路径由业务侧先完成挂载（如 `bsp_sdcard_mount(sd, "/sdcard")`），再通过 `shell_mount_add("/sdcard")` 暴露给 shell。

路径规则：

- 支持绝对路径和相对路径
- 支持 `.` 与 `..`
- 内部会做路径规范化（消除重复 `/`、处理 `.`/`..`）

## 3. 生命周期约束（重要）

- `shell_mount_add()` / `shell_mount_remove()` 只能在 `shell_init()` 前调用。
- shell 初始化后，挂载表只读；运行中不能动态增删挂载点。
- `shell_deinit()` 会停止并销毁 REPL，同时重置 cwd 与挂载表。
- `shell_init()` 的 `initial_path` 如果不存在或不是目录，会自动回退到 `/`。

## 4. 快速接入示例

```c
#include "bsp_sdcard.h"
#include "shell.h"

void app_main(void)
{
    bsp_sdcard_handle_t sd = NULL;
    ESP_ERROR_CHECK(bsp_sdcard_open(&sd));
    ESP_ERROR_CHECK(bsp_sdcard_mount(sd, "/sdcard"));

    ESP_ERROR_CHECK(shell_mount_add("/sdcard"));

    shell_cfg_t shell_cfg = shell_default_config();
    shell_cfg.prompt = "jlc$ ";
    shell_cfg.initial_path = "/sdcard";
    ESP_ERROR_CHECK(shell_init(&shell_cfg));
}
```

可直接参考可运行示例：`components/shell/test_app/main/test_shell.c`。

## 5. 常见返回值与排查

- `ESP_ERR_INVALID_STATE`
  - 重复 `shell_init()`
  - 在 shell 已启动后调用 `shell_mount_add/remove()`
  - 未初始化就执行依赖运行态的操作
- `ESP_ERR_INVALID_ARG`
  - 挂载路径不是绝对路径（不以 `/` 开头）
  - 挂载路径为根路径 `/`（保留）
- `ESP_ERR_NO_MEM`
  - 挂载点数量超过上限（当前实现上限为 8）
- `ESP_ERR_NOT_FOUND`
  - 删除不存在的挂载点
  - `cd` 到不存在路径

## 6. 设计边界

该组件当前定位是“最小可用 shell”：

- 提供 REPL 生命周期管理
- 提供基本目录浏览命令
- 不负责存储介质驱动与文件系统挂载（由业务组件负责）
- 不提供复杂命令框架与权限模型

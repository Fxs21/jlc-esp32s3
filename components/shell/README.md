# shell 组件使用说明

`shell` 是一个基于 ESP-IDF `esp_console` 的轻量命令行组件.
它提供最小文件系统浏览能力和可选 BSP 调试命令, 适合在串口或 USB 控制台做板级调试.

它不是 BSP: BSP 不依赖 shell, 根应用也不必依赖 shell; shell 通过 BSP public API 暴露调试命令, 用于自己的 `test_app` 或临时调试应用.

## 1. 当前能力范围

对外 API 位于 `components/shell/include/shell.h`:

- `shell_default_config()`
- `shell_init()` / `shell_deinit()`
- `shell_mount_add()` / `shell_mount_remove()`
- `shell_register_bsp_commands()`

当前内置文件系统命令:

- `pwd`: 打印当前目录
- `cd [path]`: 切换目录
- `ls [path]`: 列出目录内容
- `cat <path>`: 打印文件内容
- `hexdump <path> [len]`: 以十六进制查看文件开头, 默认 256 字节, 最大 4096 字节
- `mkdir <path>`: 创建目录
- `rm <path>`: 删除文件或空目录

当前内置诊断命令:

- `i2c scan` / `i2c_scan`: 扫描 BSP 默认 I2C bus 的 7-bit 地址; 固定使用 board 默认总线, 不接受自定义 pin

当前可选 BSP 调试命令:

- `bsp info` / `bsp desc`: 打印 BSP 能力摘要 (不含 display/camera/audio/PMU 信息 -- 此类复杂外设由自身 `test_app` 验证)
- `imu read [count]`: 打开 IMU, 连续读取 `count` 次数据 (`data_ready` 轮询), 然后关闭; count 默认 1
- `touch read`: 打开 touch, 读取一次触点, 然后关闭
- `gnss read [count] [timeout_ms]`: 打开 GNSS, 连续读取 `count` 条 NMEA 语句 (`timeout_ms` 单次超时), 然后关闭; count 默认 1, 最大 100
- `backlight set <percent>`: 设置背光百分比, 范围 `0..100`
- `sd info [mount_path]` / `sd fs [mount_path]`: 打印 SD/FAT 文件系统容量信息

设计原则: shell 只保留简单 I2C/UART 外设 (IMU/Touch/GNSS) 和通用调试命令 (backlight/SD). display, camera, audio, PMU 等复杂外设应使用对应的 `test_app` 验证.

## 2. 路径与挂载模型

- shell 的根目录固定为 `/`, 它是一个虚拟根.
- 在 `/` 下执行 `ls` 时, 显示的是已注册挂载点, 例如 `sdcard/`.
- 真实文件系统路径由业务侧先完成挂载, 例如 `bsp_sdcard_mount(sd, "/sdcard")`, 再通过 `shell_mount_add("/sdcard")` 暴露给 shell.

路径规则:

- 支持绝对路径和相对路径.
- 支持 `.` 与 `..`.
- 内部会做路径规范化, 消除重复 `/`, 处理 `.` / `..`.

## 3. 生命周期约束

- `shell_mount_add()` / `shell_mount_remove()` 只能在 `shell_init()` 前调用.
- shell 初始化后, 挂载表只读; 运行中不能动态增删挂载点.
- `shell_deinit()` 会停止并销毁 REPL, 同时重置 cwd 与挂载表.
- `shell_init()` 的 `initial_path` 如果不存在或不是目录, 会自动回退到 `/`.
- `shell_cfg_t.enable_bsp_commands` 为 `true` 时, `shell_init()` 会在 REPL 启动前注册 BSP 调试命令.
- `shell_register_bsp_commands()` 是低层注册函数, 通常不需要业务侧直接调用.

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
    shell_cfg.enable_bsp_commands = true;
    ESP_ERROR_CHECK(shell_init(&shell_cfg));
}
```

可直接参考可运行示例: `components/bsp/test_app/shell/main/test_shell.c`.
该示例会打印当前编译进来的 BSP board 名称; 如果当前 board 的 SD 尚未实现, 会自动从 `/` 启动 shell, 便于先运行 `i2c scan` 等无文件系统依赖的诊断命令.

### 多板型配套运行

shell 的测试入口是 `components/bsp/test_app/shell`, 它不单独选择硬件, 板型由 `components/bsp` 的 Kconfig 决定. 用 `bsp.sh` 在 DoerS3 / AuraS3 之间切换:

```sh
cd components/bsp/test_app

./bsp.sh shell doer build flash monitor
./bsp.sh shell aura build flash monitor
```

`bsp.sh` 为每个 app 生成 app-local `sdkconfig` 和 `build/`, 并在切换 board 时自动清理两者, 不需要手工删除旧配置. AuraS3 通过板载 USB 直连时, 它会额外写入 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, 让 `esp_console` REPL 走 USB Serial/JTAG 控制台.

启动后可先看日志里的 BSP board 名称, 再执行 `bsp info` / `i2c scan` 交叉确认.

## 5. 设计边界

该组件当前定位是调试工具, 不是产品功能框架:

- 提供 REPL 生命周期管理.
- 提供基本目录浏览命令.
- 提供可选 BSP 调试命令.
- BSP 调试命令只使用 BSP public API.
- BSP 调试命令默认按命令临时 `open` / 操作 / `close`.
- 需要保持硬件状态的命令可以持有调试 handle, 例如 `backlight set` 会保持 PWM 输出.
- 不负责存储介质驱动与文件系统挂载, 由业务组件负责.
- 不提供复杂命令框架, 脚本语言, 权限模型或后台任务管理.

## 6. 常见返回值与排查

- `ESP_ERR_INVALID_STATE`: 重复 `shell_init()`, 在 shell 已启动后调用 `shell_mount_add/remove()`, 或未初始化就执行依赖运行态的操作.
- `ESP_ERR_INVALID_ARG`: 挂载路径不是绝对路径, 挂载路径为根路径 `/`, 或命令参数非法.
- `ESP_ERR_NO_MEM`: 挂载点数量超过上限, 当前实现上限为 8.
- `ESP_ERR_NOT_FOUND`: 删除不存在的挂载点, 或 `cd` 到不存在路径.
- `ESP_ERR_NOT_SUPPORTED`: 当前 board 不支持对应 BSP 外设.

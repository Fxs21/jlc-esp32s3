# jlc-esp32s3

ESP32-S3 双板工程,目标硬件是 DoerS3 和 AuraS3.

## 1. 仓库目的

同一套 LVGL 应用要跑在两块 pin,bus,芯片和上下电时序完全不同的板子上. 如果 app 直接写 GPIO,寄存器或芯片驱动,换一块板就要重写一遍.

所以本仓库分两层:

- BSP 层 `components/bsp`: 消化两块板的硬件差异,对上层暴露同一组 `bsp_xxx` public API.
- 应用层: 只调用 `bsp_xxx`,不 include 板级头文件,不碰芯片寄存器,换板不改代码.

BSP 是手段,不是目的. 目的有两个:

1. 让同一个 app 在 DoerS3 / AuraS3 上都能稳定运行.
2. 用真实应用持续验证 BSP.

正式承载的应用还没有定;选定前 BSP 只保证 `components/bsp/test_app` 已验证的能力.

## 2. 当前状态

| 目标板 | 状态 |
|---|---|
| DoerS3 | 主验证板. display,touch,backlight,sdcard,imu,audio,camera,gnss 全部真机通过 |
| AuraS3 | 外设已接入并真机验证. 无 camera;GNSS 硬件未连接,待验证 |

逐项能力见 `docs/bsp/capabilities.md`;真机结论和时间线见 `docs/bsp/status.md`.

验证入口是 `components/bsp/test_app/*`: `audio`,`camera`,`pmu`,`shell`,`ui`.

## 3. 仓库构成

```text
components/bsp/       # 唯一对外交付组件: public API + board port + 私有 driver + test_app
components/shell/     # 独立调试 shell,不属于 BSP
main/                 # 应用入口壳,能力验证走 components/bsp/test_app
docs/                 # 设计,状态,接入指南和硬件事实
```

`main/` 当前是空入口,只为让 root 工程能被 `idf.py build`;不要在 `main/` 里堆能力验证代码.

## 4. BSP 使用边界

- app 只 include `components/bsp/include/` 下的头文件.
- public API 只暴露 `esp_err_t`,基础 C 类型和 BSP 自有 `struct` / `enum`.
- 原生对象只通过明确命名的 escape hatch 暴露,例如 `bsp_ui_get_lvgl_display()`,`bsp_i2c_acquire()`.
- 不做 runtime board detect,不做 board database,不做通用 bus HAL.
- 板级差异只存在于 `components/bsp/src/boards/<board>/`.

完整设计说明见 `docs/bsp_design.md`.

## 5. 快速开始

```sh
source ~/esp/esp-idf/export.sh
idf.py build
```

BSP test app:

```sh
cd components/bsp/test_app
./bsp.sh ui doer build flash monitor
./bsp.sh audio aura build flash monitor
```

board 由 Kconfig 选择,见 `components/bsp/Kconfig`:

```text
CONFIG_BSP_BOARD_DOERS3
CONFIG_BSP_BOARD_AURAS3
CONFIG_BSP_ENABLE_CAMERA
```

## 6. 提交前检查

```sh
tools/check.sh                # API 边界,依赖和残留符号检查
CHECK_BUILD=1 tools/check.sh  # 追加 idf.py build
```

## 7. 文档索引

| 文档 | 内容 |
|---|---|
| `docs/bsp/capabilities.md` | 双板能力矩阵和 public API 一览 |
| `docs/bsp/status.md` | 当前进度,已验证项,暂停项和下一步 |
| `docs/bsp_design.md` | BSP 结构,分层职责和 API 语义 |
| `docs/bsp/porting-guide.md` | 新增 board port 的接入指南 |
| `docs/hw/boards/doers3_truth_table.md` | DoerS3 硬件事实 |
| `docs/hw/boards/auras3_truth_table.md` | AuraS3 硬件事实 |
| `docs/hw/auras3-display-te.md` | AuraS3 TE 防撕裂实验记录 |
| `docs/hw/auras3-pmu-key.md` | AuraS3 PMU / KEY2 阶段性结论 |
| `components/shell/README.md` | 调试 shell 使用说明 |
| `AGENTS.md` | coding agent 协作规则 |

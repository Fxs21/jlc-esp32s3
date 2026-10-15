# jlc-esp32s3

ESP32-S3 双板工程,目标硬件是 DoerS3 和 AuraS3;正式承载应用是 xtrack,同一份 App 源码同时跑 PC 模拟器和真机.

## 1. 仓库目的

xtrack 要跑在 PC 模拟器和两块 pin,bus,芯片和上下电时序完全不同的板子上. 如果 app 直接写 GPIO,寄存器或芯片驱动,换一个后端就要重写一遍.

所以本仓库分三层:

- App 层 `xtrack/`: 平台无关,只依赖 LVGL 和 `xtrack/hal/` 的接口,换后端不改代码.
- 后端: 板侧 `main/` (调 BSP public API),PC 侧 `sim/` (SDL2 显示/输入 + 模拟数据). 两侧编同一份 App 源码, 源清单唯一出处是 `xtrack/app_sources.cmake`.
- BSP 层 `components/bsp`: 消化两块板的硬件差异,对上层暴露同一组 `bsp_xxx` public API.

BSP 是手段,不是目的. 目的有两个:

1. 让 xtrack 在 DoerS3 / AuraS3 / PC 模拟器上都能稳定运行.
2. 用真实应用持续验证 BSP.

## 2. 当前状态

| 后端 | 状态 |
|---|---|
| PC 模拟器 `sim/` | 可编可跑: LVGL 9.5 + SDL2,466x466 圆屏,启动页 -> 表盘页流程正常 |
| 板侧 `main/` | 未接入: 板侧 HAL 和 UI 接线还没写,只保证 root 工程 `idf.py build` 通过 |

App 进度,已知限制和下一步见 `docs/app/status.md`;结构和边界见 `docs/app/design.md`.

BSP 已验证能力:

| 目标板 | 状态 |
|---|---|
| DoerS3 | 主验证板. 除 `audio`,`key` 外全部模块走完新式自检闭环;`audio` 已对齐会话模型,未按新式自检复测;`key` 首次真机 4/4 PASS,待干净 sha 复跑和例程删除 |
| AuraS3 | 可测模块都走了新式自检;待补: pmu 电池场景 (无电池), audio 喇叭响度 (未接喇叭), key 自检;无 camera,GNSS 模组未贴装 |

逐项能力见 `docs/bsp/capabilities.md`;进度见 `docs/bsp/status.md`,真机结论和测量数据见各板 truth table.

每个模块的验证入口是 `test/<module>/` 自检 app, 统一入口 `test/bsp.sh`;清单和使用方式见 `test/README.md`.

## 3. 仓库构成

```text
xtrack/               # 承载应用 App 层,平台无关;IDF 组件和 PC 模拟器编同一份源码
sim/                  # PC 模拟器后端: 入口,SDL 显示/输入,模拟 HAL,lv_conf.h,LVGL submodule
main/                 # 板侧后端: 应用入口和板侧 HAL (待接入)
components/bsp/       # 唯一对外交付组件: public API + board port + 私有 driver
components/shell/     # 独立调试 shell,不属于 BSP
test/                 # 板级自检工程,bsp.sh 为统一构建入口
docs/                 # 设计,状态,接入指南,硬件事实和板厂例程参考
```

`main/` 当前是空入口,只为让 root 工程能被 `idf.py build`;xtrack 板侧接入是下一里程碑, 不要在 `main/` 里堆能力验证代码.

## 4. BSP 使用边界

- app 只 include `components/bsp/include/` 下的头文件.
- public API 只暴露 `esp_err_t`,基础 C 类型和 BSP 自有 `struct` / `enum`.
- 原生对象只通过明确命名的 escape hatch 暴露,例如 `bsp_ui_get_lvgl_display()`,`bsp_i2c_acquire()`.
- 不做 runtime board detect,不做 board database,不做通用 bus HAL.
- 板级差异只存在于 `components/bsp/src/boards/<board>/`.

完整设计说明见 `docs/bsp/design.md`.

## 5. 快速开始

固件 (root 工程):

```sh
source ~/esp/esp-idf/export.sh
idf.py build
```

PC 模拟器:

```sh
git submodule update --init --depth 1   # 首次
cmake -S sim -B sim/build -G Ninja
cmake --build sim/build
./sim/build/lvgl-sim                    # 需要图形环境 (WSLg 可用)
```

BSP test app:

```sh
cd test
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
tools/check.sh                # API/边界,依赖,残留符号和退役名检查
CHECK_BUILD=1 tools/check.sh  # 追加 idf.py build
CHECK_SIM=1 tools/check.sh    # 追加 PC 模拟器构建
```

## 7. 文档索引

全部文档,代码入口和验证入口索引见 `docs/README.md`.

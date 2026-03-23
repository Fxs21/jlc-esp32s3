# jlc-esp32s3

ESP32-S3 board support project for DoerS3 and AuraS3.

当前重点是维护 `components/bsp`,为不同开发板提供统一,稳定,简单的 BSP API.应用层只依赖公开 BSP API,不直接感知具体开发板的 pin,bus,chip driver 或 power sequence.

## 当前状态

- DoerS3 是当前主要验证板.
- AuraS3 保留相同 BSP API 形态,部分能力仍为 stub.
- BSP 已收敛为单一组件:`components/bsp`.
- App 应通过 `bsp_xxx` public API 使用外设能力.
- 小 IC driver 放在 BSP 私有实现中,不暴露给 app.

详细状态见:

- `docs/bsp/status.md`
- `docs/bsp_design.md`
- `docs/hw/boards/doers3_truth_table.md`
- `docs/hw/boards/auras3_truth_table.md`

## 目录结构

```text
components/
  bsp/        # 统一 BSP 组件
  shell/      # 独立调试 shell,不属于 BSP

docs/
  bsp/        # BSP 文档入口和当前状态
  hw/boards/  # board 硬件事实表

main/         # 当前 root app 入口
```

## BSP 设计原则

- 公开 API 保持简单,稳定,可解释.
- 核心 BSP API 只暴露 `esp_err_t`,基础 C 类型和 BSP 自有类型.
- 不在核心 API 中直接暴露 ESP-IDF,LVGL 或第三方 driver 类型.
- 板级差异由 `components/bsp/src/boards/<board>/` 消化.
- 小芯片 driver 放在 `components/bsp/src/drivers/`,作为 BSP 私有实现.
- 不做 runtime board detect,不做 board database,不做通用 bus HAL.

## 构建

进入 ESP-IDF 环境:

```sh
source ~/esp/esp-idf/export.sh
```

构建 root app:

```sh
idf.py build
```

## BSP Test Apps

每个 BSP 能力都有独立 test app:

```text
components/bsp/test_app/board
components/bsp/test_app/display
components/bsp/test_app/touch
components/bsp/test_app/sdcard
components/bsp/test_app/gnss
components/bsp/test_app/imu
components/bsp/test_app/audio
components/bsp/test_app/camera
components/bsp/test_app/ui
```

构建示例:

```sh
idf.py -C components/bsp/test_app/board build
idf.py -C components/bsp/test_app/touch build
```

部分 test app 需要真机和人工观察,具体状态见 `docs/bsp/status.md`.

## Board 选择

BSP 通过 Kconfig 选择目标 board:

```text
CONFIG_BSP_BOARD_DOERS3
CONFIG_BSP_BOARD_AURAS3
```

DoerS3 是当前主要验证目标.AuraS3 的真实硬件适配仍在补齐中.

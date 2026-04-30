# BSP 设计边界

当前 BSP 采用一个公开组件、多个板级实现的结构。

## 原则

1. 公共 API 统一。
2. 板级实现分离。
3. 不提前抽象。
4. Kconfig 只选择板子。
5. 私有硬件 glue 不暴露到 `components/` 顶层。

## 目标结构

```text
components/
  bsp/
    include/              # app 可见 API
    src/common/           # 公共组合层或占位层
    src/boards/doers3/    # DoerS3 实现
    src/boards/auras3/    # AuraS3 实现或占位
    src/drivers/          # 可复用 IC driver
  shell/              # 非 BSP，作为独立调试/命令组件
```

## 板级实现

每块板子实现相同的公共接口：

- `bsp_board_get_info()`
- `bsp_display_*()`
- `bsp_touch_*()`
- `bsp_backlight_*()`
- `bsp_sdcard_*()`
- `bsp_gnss_*()`
- `bsp_imu_*()`
- `bsp_audio_*()`
- `bsp_camera_*()`

`bsp_board_get_info()->capabilities` 只表示当前 BSP 已接入、可通过公共 API 使用的能力，不提前声明尚未实现的硬件。

DoerS3 和 AuraS3 不共享一个大 truth table。具体 GPIO、bus、IOEXP、屏幕方向、SDMMC 接线等，写在各自 board port 中。

## IOEXP

IOEXP 不作为公开模块。

当前规则：

- DoerS3 的 PCA9557 只属于 DoerS3 内部基础设施。
- 上层不 include `ioexp` 头文件。
- Display 的 LCD CS 只在 display open/close 时控制，不参与每笔 SPI transaction。
- 如果后续多个板子都需要同类 IOEXP，再考虑提炼公共 driver；现在不抽。

## API 命名

外设生命周期统一使用 `open/close`。

当前规则：

- 有描述信息的外设提供 `bsp_xxx_get_desc()`。
- 有运行态资源的外设提供 `bsp_xxx_open()` / `bsp_xxx_close()`。
- 不再使用 `new/del` 命名，避免和 C++ 对象生命周期混在一起。

## Kconfig

Kconfig 只选择当前板子：

```text
CONFIG_BSP_BOARD_DOERS3
CONFIG_BSP_BOARD_AURAS3
```

不在 Kconfig 中配置每个 GPIO、屏幕尺寸、UART 口或 I2C 地址。

## 当前取舍

- 不做 `bsp_ctrl_pin`。
- 不做大 `bsp_boarddb`。
- 不做 `bsp_hal_spi/i2c/uart/i2s` 通用层。
- 不做运行时 backend registry。
- 优先保证结构清楚，然后逐个迁移真实设备实现。

## Test App

每个 BSP 能力都放在 `components/bsp/test_app/<name>` 下单独验证。

当前规则：

- test_app 只依赖公开 `bsp` API，不 include board port 或内部 driver 头文件。
- 默认板子选择为 DoerS3：`CONFIG_BSP_BOARD_DOERS3=y`。
- 构建入口统一使用 `idf.py -C components/bsp/test_app/<name> build`。
- 需要硬件动作的测试在 app 中打印结果，不把人工步骤藏进 BSP 代码。

## Shell

`shell` 不属于 BSP。

当前处理规则：

- 保留为独立调试组件：`components/shell`。
- BSP 不依赖 shell，根应用默认也不依赖 shell。
- shell 的 `test_app` 可以依赖 `bsp`，用于人工调试文件系统、SD 卡或后续板级命令。
- 不把 shell 命令注册塞进 `components/bsp`，避免 BSP 变成应用框架。

如果后续 master 只想保留纯 BSP，可以把 shell 移到 `tools/` 或独立仓库；当前先不移动，避免破坏已有 shell test_app。

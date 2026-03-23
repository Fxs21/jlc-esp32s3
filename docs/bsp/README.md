# BSP 文档总览

当前 BSP 按“board truth table + 公共基础设施 + 设备组件 + UI 入口”组织。

## 入口

- 架构说明：`docs/bsp/architecture.md`
- 当前状态：`docs/bsp/status.md`
- 板级文档：`docs/bsp/boards/`
- 组件文档：`docs/bsp/components/`
- 硬件真值表：`docs/hw/boards/`
- `DoerS3` 真值表：`docs/hw/boards/doers3_truth_table.md`

## 当前板型

- `DoerS3`：当前已落地的板级配置
- `AuraS3`：已预留编译期开关，当前仅保留空占位，后续补充真实接线

## 当前组件分层

- board truth table：`bsp_boarddb`
- shared infra：`bsp_i2c_bus`、`bsp_ioexp`
- devices：`bsp_display`、`bsp_touch`、`bsp_backlight`、`bsp_audio`、`bsp_sdcard`、`bsp_camera`、`bsp_imu`、`bsp_gnss`
- ui：`bsp_ui`
- utility：`shell`

## 构建

- 进入 ESP-IDF 环境：`source ~/esp/esp-idf/export.sh`
- 仓库根目录构建：`idf.py build`
- 不使用：`idf.py -B build_ninja build`

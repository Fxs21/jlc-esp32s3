# BSP 当前状态

本文档只记录当前 BSP 的落地状态、缺口和下一步方向。

## 已完成

- `bsp_boarddb` 已作为唯一板级 truth table 入口：
  - 公共入口：`components/bsp_boarddb/src/bsp_boarddb.c`
  - DoerS3 描述：`components/bsp_boarddb/src/boards/doers3.c`
  - AuraS3 占位：`components/bsp_boarddb/src/boards/auras3.c`
- DoerS3 的 UI 相关数据已拆成 `display/touch/backlight` 三份描述。
- DoerS3 的 `ioexp/sdcard/gnss/imu/audio/camera` 描述已进入 `bsp_boarddb`。
- AuraS3 已接入 board 选择和空 truth table，占位状态下不声明 UI 能力。
- `bsp_i2c_bus` 已替代旧 `bsp_bus` 命名，职责只表达共享 I2C bus。
- `bsp_ioexp` 已保留为板级 IO 扩展入口，公共 API 为 `open/close/set_level`。
- PCA9557 寄存器操作已拆到 `bsp_ioexp` 私有 driver：
  - `components/bsp_ioexp/src/drivers/bsp_ioexp_pca9557.c`
  - `components/bsp_ioexp/src/drivers/bsp_ioexp_pca9557.h`
- `bsp_sdcard` 已从硬编码配置收敛到 `bsp_boarddb`，公共 API 为 `open/close/mount/unmount/get_info/get_fs_info`。
- GPS 组件命名已收敛为 `bsp_gnss`，当前只提供带 timeout 的 UART raw bytes 读取，不内置 NMEA parser。
- `bsp_ui` 是当前 UI 唯一组合入口，拥有 `display/touch/backlight` 生命周期。
- 旧的过程类文档和旧 LVGL 组件文档已移除，当前文档只描述现状和后续方向。

## 未完成

- AuraS3 真实硬件 truth table 尚未补齐；当前不能真正运行 UI，这是预期状态。
- `bsp_power` 尚未落地；AXP2101 仍只在设计方向中。
- `bsp_hal_spi`、`bsp_hal_i2s`、`bsp_hal_uart` 尚未落地；当前只有 `bsp_i2c_bus` 已实际作为共享基础设施存在。
- `bsp_audio`、`bsp_camera`、`bsp_imu` 仍保留部分历史 API，例如 `new/del` 或 `present()`。
- GNSS 暂不解析定位结构，也不做 line buffering，只读取 UART 字节流。
- 每个组件独立 `test_app` 的完整验收还未统一梳理；当前只保证根目录构建作为自动化门槛。
- 真机行为仍需按外设逐项确认，尤其是 SD 卡挂载、GNSS 收星、音频、摄像头和 AuraS3 后续硬件。

## 下一步

1. 补齐 AuraS3 的真实 board truth table，先确认 `display/touch/backlight/power`。
2. 设计并落地 `bsp_power`，优先服务 AuraS3 的电源和背光路径。
3. 继续统一设备 API，把剩余组件收敛到 `get_desc/open/close/read/write/get_frame/set_xxx`。
4. 按优先级收敛 `bsp_imu`、`bsp_audio`、`bsp_camera` 的 API 和 boarddb 使用方式。
5. 视共享资源复杂度决定是否引入 `bsp_hal_spi`、`bsp_hal_i2s`、`bsp_hal_uart`。
6. 在需要结构化定位数据时，为 `bsp_gnss` 增加 NMEA parser 或 parser 适配层。
7. 统一整理各组件 `test_app`，形成可重复的最小硬件验证清单。

## 当前验证

- 仓库根目录构建通过：`source ~/esp/esp-idf/export.sh && idf.py build`
- `x-track` 已适配 `bsp_ui/bsp_gnss/bsp_sdcard` 新接口，并随根目录构建通过。
- DoerS3 的 `bsp_ui/test_app` 构建通过：`idf.py -C components/bsp_ui/test_app -D SDKCONFIG=/tmp/sdkconfig.doers3 build`

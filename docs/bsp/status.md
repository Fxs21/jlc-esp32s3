# BSP 当前状态

本文档只记录 `components/bsp` 的当前状态,已验证内容,缺口和下一步.长期设计原则见 `docs/bsp_design.md`.

## 当前目标

- 保持一个公开 BSP 组件: `components/bsp`.
- app 面向统一 BSP API 编写,不直接依赖 board port 或私有 driver.
- DoerS3 和 AuraS3 都按同一组 public API 接入,board 差异由 board port 消化.
- 每个 BSP 能力通过 `components/bsp/test_app/<name>` 单独验证.
- shell 作为手动 bring-up/debug 工具存在于 `components/bsp/test_app/shell`,但 `components/bsp` 不依赖 shell.

## 当前结构

- `components/bsp/include/`: 公开 BSP API.
- `components/bsp/src/common/`: 跨板复用组合逻辑,包括 UI lifecycle,I2C bus owner,LVGL buffer helper 和 unsupported stubs.
- `components/bsp/src/boards/doers3/`: DoerS3 board port.
- `components/bsp/src/boards/auras3/`: AuraS3 board port.
- `components/bsp/src/drivers/`: BSP 私有 IC driver.
- `components/bsp/test_app/`: BSP test_app 和 shell 调试 app.

## CMake / board 选择

- `components/bsp/CMakeLists.txt` 按 `CONFIG_BSP_BOARD_DOERS3` / `CONFIG_BSP_BOARD_AURAS3` 直接 include 对应 `board.cmake`.
- 每块板的 source manifest 写在 `components/bsp/src/boards/<board>/board.cmake`.
- 不使用 `BSP_BOARD_HAS_*` capability auto CMake framework.
- test_app 使用 `components/bsp/test_app/bsp.sh <app> <board> [idf action...]`.
- 每个 test app 只有一个 `sdkconfig.defaults`,一个 app-local `sdkconfig`,一个 app-local `build/`.

## Public API 当前能力

- `bsp_board`: board id,name,capabilities.
- `bsp_display`: 低层 board-native async transfer API,只表达 native area write + wait.
- `bsp_ui`: application UI entry,组合 LVGL display,touch,backlight.
- `bsp_touch`: touch point 读取.
- `bsp_backlight`: 0..100 percent 亮度语义.
- `bsp_sdcard`: mount/unmount 和挂载信息.
- `bsp_imu`: accel,gyro,temp 读取.
- `bsp_audio`: ES8311 playback + ES7210 record 的最小句柄模型.
- `bsp_gnss`: UART/NMEA byte stream.
- `bsp_camera`: DoerS3 camera 单帧采集,AuraS3 返回 unsupported.
- `bsp_pmu`: AuraS3 AXP2101 只读状态和事件,DoerS3 返回 unsupported.

## Display / UI 当前语义

- `bsp_display_open()` 固定 board-native contract,不承诺统一 host RGB565 byte order.
- `bsp_display_write_async()` 只发送 native 数据,`data_size` 只做内存安全校验.
- `bsp_display_wait_transfer_done()` 无 timeout.
- transfer done callback 只允许一个 owner.
- public display API 不提供 `fill` 或 colorbar helper,shell 也不提供 display 命令.
- `bsp_display_open()` 和 `bsp_ui_open()` 互斥;`bsp_ui_open()` 复用 `bsp_display_open()`.
- `bsp_ui_process()` 直接返回 `lv_timer_handler()` 的 delay,不在 BSP 内硬限制 FPS.
- UI test 使用 `lv_demo_widgets()` 作为真实压力测试,UI task priority `6`,stack `16384`,LVGL log off.

## DoerS3 当前能力

- Board: board info / capabilities 可读取.
- Display: ST7789 已接入,当前 native display contract 为 little-endian RGB565,LVGL flush 不做 byte swap.
- Touch: FT6336 已接入.
- Backlight: LEDC backlight 已接入.
- SD card: 1-bit SDMMC 挂载已接入.
- IMU: QMI8658 已接入,可读取 accel,gyro,temp.
- Audio: ES8311 speaker playback + ES7210 MIC1/MIC2 16-bit stereo record 已接入.
- Camera: GC0308 QVGA RGB565 单帧采集已接入.
- GNSS: MAX-M10S 使用 `UART1`,`38400 8N1`,可收到 NMEA ASCII.

## AuraS3 当前能力

- Board: capabilities 当前为 display,touch,backlight,imu,audio,gnss,sdcard,pmu;camera 不存在.
- Display: CO5300 QSPI AMOLED `466x466`,native contract 为 high-byte-first RGB565 stream,align `2x2`,gap `6,0`;init table 已按厂家序列收敛为 `FE 00` / `C4 80` / `3A 55` / `35 00` / `53 20` / `51 00` / `63 FF` / `2A` / `2B` / `11` delay `60ms` / `29`.
- UI: LVGL PARTIAL render,dirty area 2-pixel rounder,RGB565 swap enabled,TE wait disabled by default.
- Backlight: CO5300 command `0x51`,public percent `0..100`,默认 brightness `0`,UI 首帧后再打开亮度.
- Touch: CST9217 已接入,方向真机确认正常.
- IMU: QMI8658 已接入,地址先探测 `0x6B`,fallback `0x6A`.
- Audio: ES8311 playback + ES7210 MIC1/MIC2 record 已接入,PA 使用 direct GPIO `46`.
- GNSS: LC76GABMD UART BSP 已接入,`UART` pin 为 RX `GPIO18`,TX `GPIO17`,baud 当前实现为 `38400`;当前硬件未连接 GPS,暂不做真机结论.
- SD card: 当前 BSP 使用 SDMMC 1-bit,`SDMMC_HOST`,CLK `GPIO2`,CMD `GPIO1`,D0 `GPIO3`;真机确认.
- PMU: AXP2101 只读 public API 已接入,支持 VBUS/电池/充电/电压/温度/status/event 读取;KEY2 短按/长按,长按硬关机,KEY2 开机,电池插入/移除和充电开始已真机确认.
- Camera: 用户确认无硬件,保持 `ESP_ERR_NOT_SUPPORTED`,不声明 `BSP_BOARD_CAP_CAMERA`.

## 已验证

- `git diff --check` 和 `tools/check.sh` 已通过.
- DoerS3 真机已确认: shell test 正常,UI test 正常.
- DoerS3 真机已确认: ST7789 little-endian native contract + LVGL flush no swap 正常.
- DoerS3 既有能力历史已确认: board,touch,sdcard,imu,camera,gnss,audio test_app 正常.
- AuraS3 真机已确认: board capabilities 正常,`camera: no`.
- AuraS3 真机已确认: IMU 初始化和读数正常.
- AuraS3 真机已确认: SD card SDMMC 1-bit mount 成功,容量和 FS 信息正常.
- AuraS3 真机已确认: shell `audio tone 1000 1000` 播放正常,ES8311/ES7210 open 正常.
- AuraS3 真机已确认: UI 启动正常,LVGL buffer 为 double buffer,`lines=59`,单 buffer `54988` bytes,优先 SRAM DMA,TE wait off.
- AuraS3 真机已确认: PMU test app 可构建运行,`KEY2` / `SYS_OUT` / `AXP_IRQ` / 电池插拔 / 充电开始 / `battery_percent` 收敛路径已完成阶段性验证.
- AuraS3 GNSS 当前因硬件未连接,只确认未接模块时 shell/test 会读不到数据.

## 不承诺 / 暂停项

- AuraS3 camera 不存在,不实现.
- AuraS3 RTC `PCF85063` 仍只是硬件事实,当前没有 public BSP API.
- AuraS3 PMU public API 只承诺只读状态和 mapped events,不承诺 software power-off,rail control 或充电参数配置.
- GNSS public API 当前只承诺 raw byte stream,不承诺结构化定位 API.
- Audio MIC3 playback reference,TDM,AEC 已暂停,当前只承诺 ES8311 playback 和 ES7210 MIC1/MIC2 stereo record.
- TE runtime 当前默认不启用;后续若研究 TE,应参考 `esp_lvgl_adapter` 的 `TE_SYNC` 路径.
- Display shell `fill` / `colorbars` 不恢复,避免把 board-native byte order 细节暴露成调试 API.

## 下一步

1. 如后续继续 PMU,先做 internal-only software power-off 验证,确认 USB,仅电池,USB+电池三种场景和 `KEY2` 重新开机行为后,再决定是否增加 public `shutdown` API.
2. 继续确认 AXP2101 rail 到 `VCC3V3` / `VCCRTC` / 外设电源的映射,验证前不开放 DCDC/LDO control.
3. ~~真机验证 AuraS3 SDMMC 1-bit SD card mount~~ 已通过.
4. 如需要,补测 AuraS3 `audio rec-rms` 的 MIC1/MIC2 录音 RMS.
5. 等 AuraS3 GNSS 硬件连接后,验证 `38400` baud,TX/RX 方向和 GPS reset 极性.
6. 设计 `bsp_rtc` public API 前,先确认 `PCF85063` 的实际产品需求.

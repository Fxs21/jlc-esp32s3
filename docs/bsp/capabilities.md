# BSP 能力矩阵

本文回答两个问题: 两块板分别支持什么,以及 app 可以用哪些 public API.

本文是 BSP 能力承诺的唯一出处: 已承诺范围和明确不承诺范围都写在这里,其他文档只引用不重复.

- 进度,已完成项和下一步: `docs/bsp/status.md`
- 硬件事实 (pin,bus,地址,连接): `docs/hw/boards/<board>/truth_table.md`
- API 语义和分层说明: `docs/bsp/design.md`

## 1. 双板能力对照

| 能力 | public API | DoerS3 | AuraS3 |
|---|---|---|---|
| board 信息 | `bsp_board_get_info()` | DoerS3 | AuraS3 |
| display | `bsp_display_*` | ST7789,SPI,`320x240` 横屏 | CO5300,QSPI,`466x466` |
| UI (LVGL) | `bsp_ui_*` | 已接入 | 已接入 |
| touch | `bsp_touch_*` | FT6336,`0x38` | CST9217,`0x5A` |
| backlight | `bsp_backlight_*` | LEDC,`GPIO42` | 面板亮度寄存器 `0x51` |
| sdcard | `bsp_sdcard_*` | 1-bit SDMMC + FAT | 1-bit SDMMC + FAT |
| imu | `bsp_imu_*` | QMI8658,`0x6A` | QMI8658,`0x6B`,fallback `0x6A` |
| audio | `bsp_audio_*` | ES8311 `0x18` + ES7210 `0x41` | ES8311 `0x18` + ES7210 `0x40` |
| gnss | `bsp_gnss_*` | MAX-M10S,UART1,`38400` | 原理图预留,板上未贴模组 |
| camera | `bsp_camera_*` | GC0308 DVP,需 `CONFIG_BSP_ENABLE_CAMERA=y` | 无硬件,返回 `ESP_ERR_NOT_SUPPORTED` |
| pmu | `bsp_pmu_*` | 无,返回 `ESP_ERR_NOT_SUPPORTED` | AXP2101 `0x34`,只读 |
| i2c 诊断 | `bsp_i2c_acquire/probe/scan` | I2C0,SDA `GPIO1`,SCL `GPIO2` | I2C1,SDA `GPIO15`,SCL `GPIO14` |

能力判断统一走 `desc`:

- 每个外设有 `bsp_xxx_get_desc()`,返回 `present` 字段;display 用 `bsp_display_get_info()`.
- `present=false` 的板子,对应 `bsp_xxx_open()` 必须返回 `ESP_ERR_NOT_SUPPORTED`,不允许返回半初始化 handle.
- app 不应用 `#ifdef` 或 Kconfig 判断当前板子,应按 `present` 做运行态分支.

## 2. 容易误用的语义边界

### Display 是 board-native,不是统一 framebuffer

- `bsp_display_write()` 只发送 board-native pixel byte stream,`data_size` 只做内存安全校验.
- 两块板 byte order 不同: DoerS3 ST7789 为 little-endian RGB565,AuraS3 CO5300 为 high-byte-first.
- app 不要假设统一 RGB565 语义;要画 UI 请用 `bsp_ui_*`.
- AuraS3 QSPI 写入有 2 像素粒度限制,dirty area 由 board port 做 2 像素 rounder.
- AuraS3 TE wait 当前默认不启用;若研究 TE,参考 `docs/hw/auras3-display-te.md`.

### UI 只有 LVGL 一种

- `bsp_ui` 组合 display,touch,backlight,是 app 的 UI 入口.
- 不做多 backend,不做 vtable 或 registry;允许暴露 LVGL 类型,函数名必须带 `lvgl`.
- `bsp_ui_open()` 复用 `bsp_display_open()`,两者互斥,不能同时持有.

### Audio 只承诺已收敛的部分

- 当前稳定语义: ES8311 speaker playback + ES7210 MIC1/MIC2 16-bit stereo record,两块板一致.
- MIC3 playback reference,TDM,AEC 已暂停,不在 public API 内.
- full-duplex (同时 playback + record) 尚未真机验证;`bsp_audio_desc_t.supports_full_duplex` 两板声明 true,验证前 app 不应依赖该路径.

### PMU 只承诺只读

- 只有 AuraS3 有 PMU: `get_status` / `get_events` 读 VBUS,电池,充电,电压,温度.
- 不暴露 AXP2101 raw register 或 TCA9554 raw 电平;充电参数,rail control,software power-off 未开放.

### GNSS 只承诺 raw byte stream

- `bsp_gnss_read()` 返回 UART 原始字节,不承诺结构化定位结果.

### RTC 尚未进入 public API

- 当前没有 `bsp_rtc`;AuraS3 的 PCF85063 只是硬件事实,未承诺任何 API.

## 3. public API 一览

下表头文件都在 `components/bsp/include/` 下.

| 头文件 | API |
|---|---|
| `bsp_board.h` | `bsp_board_get_info()` |
| `bsp_display.h` | `bsp_display_open` / `close` / `get_info` / `set_done_cb` / `write` |
| `bsp_ui.h` | `bsp_ui_open` / `process` / `set_backlight` / `close`,`bsp_ui_get_lvgl_display`,`bsp_ui_get_lvgl_indev` |
| `bsp_touch.h` | `bsp_touch_get_desc` / `open` / `close` / `read` |
| `bsp_backlight.h` | `bsp_backlight_get_desc` / `open` / `close` / `set_percent` / `get_percent` |
| `bsp_sdcard.h` | `bsp_sdcard_get_desc` / `open` / `close` / `mount` / `unmount` / `get_mount_point` / `get_info` / `get_fs_info` |
| `bsp_imu.h` | `bsp_imu_get_desc` / `open` / `close` / `read` / `is_data_ready` |
| `bsp_audio.h` | `bsp_audio_get_desc` / `default_config` / `open` / `close`,`play_start` / `play_stop` / `play_set_volume` / `play_write`,`record_start` / `record_stop` / `record_set_gain` / `record_read` |
| `bsp_gnss.h` | `bsp_gnss_get_desc` / `open` / `close` / `read` |
| `bsp_camera.h` | `bsp_camera_get_desc` / `open` / `close` / `capture` / `release_frame` |
| `bsp_pmu.h` | `bsp_pmu_get_desc` / `open` / `close` / `get_status` / `get_events` |
| `bsp_i2c.h` | `bsp_i2c_acquire` / `release` / `probe` / `scan` |

统一约定:

- 生命周期用 `open` / `close`;运行态用 `read` / `write` / `start` / `stop` / `capture` / `mount`.
- `open()` 失败时必须把 `*handle_out` 置为 `NULL`;`close(NULL)` 返回 `ESP_ERR_INVALID_ARG`.
- 带超时的 IO 由调用者传 `timeout_ms`,并返回实际长度.
- public API 不承诺线程安全: 同一个 handle 由一个 owner task 串行使用,跨 task 共享由 app 自己加锁.

## 4. 明确不做

- 不做 runtime board detect,不做 `boarddb`,不做通用 `bsp_hal_*`;app 按 `present` 做运行态分支,不用 `#ifdef` 或 Kconfig 判断板子.
- 不把 Kconfig 变成板级配置表: 只选 board 和 `CONFIG_BSP_ENABLE_CAMERA`,不在 Kconfig 里配 GPIO,屏幕尺寸或器件地址.

其余架构禁项和演进规则以 `AGENTS.md` §3 和 `docs/bsp/design.md` §12 为准.

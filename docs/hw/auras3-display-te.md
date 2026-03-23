# AuraS3 Display TE 防撕裂记录

AuraS3 的 CO5300 AMOLED TE 信号接到 ESP32-S3 `GPIO13`.当前 BSP 只保留硬件宏 `AURAS3_LCD_TE GPIO_NUM_13` 和 CO5300 init table 中的 `0x35,0x00` TEON 作为板级事实,不在默认 display/LVGL flush 路径里启用 TE wait.当前 CO5300 初始化序列已按厂家给出的 QSPI/RGB565 序列收敛,只保留默认 command page,QSPI mode,RGB565,TEON,brightness control,窗口范围,`SLPOUT` delay `60ms` 和 `DISPON`.

## 结论

TE 防撕裂的关键不是等到 TE 本身,而是 TE 后要尽快开始写屏.

正确顺序:

```text
render / rgb565 swap / prepare buffer
wait TE
write pixels to panel
```

避免顺序:

```text
wait TE
render / rgb565 swap / prepare buffer
write pixels to panel
```

原因: 全屏 RGB565 buffer 是 `466 * 466 * 2 = 434312` bytes.如果在 TE 后再做 `lv_draw_sw_rgb565_swap()`,会浪费同步窗口,导致真正写屏开始得太晚.实测撕裂线固定在屏幕中间,说明 panel 扫描线稳定追上了 MCU 写入进度,这是相位问题.

## 实测过程

- 无 TE: 表格横向滑动撕裂明显.
- TE + 50-line partial flush: TE 有效,FPS 被限制在 60Hz 附近,但仍撕裂.
- TE + full refresh: 撕裂减轻,但中间仍明显.
- `rgb565 swap -> wait TE -> write`: full refresh 实验中撕裂基本消失.

## 官方 adapter 的 TE_SYNC 路径

`esp_lvgl_adapter` 已经实现 GPIO TE 同步路径,但官方 AuraS3 demo 当前配置仍是 `ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE`,因此 demo 没有实际使用这条路径.

相关实现位置:

```text
managed_components/espressif__esp_lvgl_adapter/include/esp_lv_adapter_display.h
managed_components/espressif__esp_lvgl_adapter/src/display/display_manager.c
managed_components/espressif__esp_lvgl_adapter/src/display/display_te_sync.c
managed_components/espressif__esp_lvgl_adapter/src/display/bridge/v9/lvgl_bridge_v9.c
```

adapter 的 `ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC` 用于 SPI/I80/QSPI 这类 `PANEL_IF_OTHER` 接口.注册 display 时,`display_manager` 会校验 TE GPIO 配置,创建 TE context,并根据分辨率,bus frequency,data lines 和 bpp 估算整屏传输时间,从而选择更合适的 VBlank 边沿/窗口策略.

TE context 包含 GPIO ISR,semaphore,last TE tick/time,TE period,TX start/done 时间和 window defer 状态.ISR 只记录 TE 到达并释放 semaphore;flush task 侧负责判断 TE 是否新鲜,是否仍在有效窗口,以及是否需要推迟到下一个 TE.

LVGL v9 的 TE flush 顺序是:

```text
begin frame
rgb565 swap / prepare color map
wait TE vsync
record TX start
esp_lcd_panel_draw_bitmap()
wait TX done callback
flush ready
```

这与 AuraS3 实测得到的有效相位一致: CPU-heavy 的 `lv_draw_sw_rgb565_swap()` 必须放在 TE 之前,TE 之后应尽快开始写屏.

需要注意: adapter 的 `TE_SYNC` 会选择 `FULL` render mode,并且要求 strict TE synchronization 的 single full-screen draw buffer.对 AuraS3 来说,单个 RGB565 full frame 是 `466 * 466 * 2 = 434312` bytes,通常需要 PSRAM.它是后续 TE 实验的重要参考,但不是一个可以无成本打开的局部刷新开关.

## 当前 BSP 取舍

AuraS3 当前优先选择干净,稳定的 LVGL/display 结构:

```text
LV_DISPLAY_RENDER_MODE_PARTIAL
common LVGL double buffer helper
lines = ceil(screen_height / 8)
SRAM DMA first, fallback PSRAM
只刷新 dirty area
flush 路径不等待 TE
```

当前真机确认的 AuraS3 LVGL buffer 参数:

```text
screen = 466 x 466
lines = 59
one buffer = 466 * 59 * 2 = 54988 bytes
total double buffer = 109976 bytes
memory = SRAM DMA
```

原因是 SRAM draw buffer 只能承载局部刷新,大 dirty area 仍可能被拆成多个 flush;即使每帧等 TE,只要写 GRAM 的总时间超过 panel 扫描窗口,panel scan-out 仍可能读到半新半旧内容.TE 可以固定撕裂相位,但不保证在 partial/慢传输路径下消除撕裂.

## Display / UI 当前实现边界

- `bsp_display` public API 是 board-native async transfer API,不提供 `fill` / `colorbars` / host-order RGB565 writer.
- AuraS3 native display contract 是 high-byte-first RGB565 byte stream.
- AuraS3 LVGL flush 必须调用 `lv_draw_sw_rgb565_swap()` 后再写 CO5300.
- LVGL dirty area 需要 2-pixel rounder,保证 CO5300 QSPI partial refresh 窗口对齐.
- `bsp_ui_process()` 不做 33fps 或 30ms clamp,直接返回 `lv_timer_handler()` 的 delay.
- UI task 当前使用 priority `6`,stack `16384`,LVGL log off.

## 传输注意事项

单次发送 434KB 曾触发:

```text
panel_io_spi_tx_color: spi transmit (queue) color failed
co5300_spi: panel_co5300_draw_bitmap: send color data failed
```

当前策略:

- CO5300 `max_transfer_sz` 配置为 full-frame byte size,但 LVGL 默认走 partial dirty flush.
- SPI transaction queue depth 当前保持 `2`;历史测试中 queue depth `3` 容易触发 `ESP_ERR_NO_MEM`.
- BSP display 层不手写逐行/逐块提交逻辑,保持 `write_async()` 语义为一次 native area write.

## 后续原则

- 默认交互路径使用 PARTIAL dirty flush,不是强制 full refresh.
- TE 代码可以作为后续实验重新加入,但不要污染默认 flush 语义.
- 如果继续研究 TE,应先让写屏时间接近或低于 16.7ms,否则 TE 只能移动/固定撕裂线.
- 后续可再评估 CO5300 line TE / scanline TE,dirty area merge 或 full-area threshold.
- 若考虑直接使用 `esp_lvgl_adapter` TE_SYNC,需要先接受 FULL render mode,full-screen buffer 和 PSRAM/传输时间成本.

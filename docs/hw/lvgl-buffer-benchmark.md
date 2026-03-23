# LVGL Buffer Allocation Benchmark

测试日期: 2026-05-10
测试硬件: DoerS3, ST7789 320x240 RGB565, SPI 80MHz
LVGL 版本: 9.5.0
测试程序: `lv_demo_benchmark` (通过 `test_app/ui`)
串口: CH340 at /dev/ttyUSB0

## 测试方案

5 种内存分配策略, 均使用双缓冲 (buf1 + buf2). `DIVISOR` 表示 `buffer_lines = height / DIVISOR`.

| 方案 | 内存区域 | DIVISOR | 每缓冲大小 | 双缓冲合计 | SPI chunks |
|:----:|:--------:|:------:|:----------:|:----------:|:----------:|
| 1 | SRAM DMA | 8 (30 lines) | 19,200 B | 38,400 B | 1 |
| 2 | SRAM DMA | 4 (60 lines) | 38,400 B | 76,800 B | 1 |
| 3 | PSRAM | 8 (30 lines) | 19,200 B | 38,400 B | 1 |
| 4 | PSRAM | 4 (60 lines) | 38,400 B | 76,800 B | 2 |
| 5 | PSRAM | 2 (120 lines) | 76,800 B | 153,600 B | 3 |

> SRAM DMA: `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`
> PSRAM: `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`

## SDK 配置与性能背景

以下配置来自 `test_app/ui/sdkconfig.defaults`, 与其他 `test_app` (pmu/audio/sdcard/etc.) 相比, 只有 `test_app/ui` 启用了这些性能相关项.

### Cache 分配

ESP32-S3 内部 512KB SRAM 的一部分被 cache RAM 占用, 不从 heap 中分配. 配置如下:

| 配置 | 默认值 | UI 试验值 | 占用 SRAM |
|------|:-----:|:---------:|:---------:|
| 指令缓存大小 | 16KB | 32KB | +16KB |
| 指令缓存方式 | 8-way | 8-way (同) | -- |
| 指令缓存行 | 32B | 32B (同) | -- |
| 数据缓存大小 | 32KB | 64KB | +32KB |
| 数据缓存方式 | 8-way | 8-way (同) | -- |
| 数据缓存行 | 32B | 64B | -- |

UI 配置合计占用 **96KB** 内部 SRAM 做 cache, 比默认多占 48KB.

- **指令缓存 32KB**: 约 8000 条 ARM Thumb 指令的热路径可以驻留 cache. 对 LVGL 渲染循环等频繁调用的函数有益.
- **数据缓存 64KB + 64B 行**: 提升 PSRAM 访问的缓存命中率. 连续大块绘制 (矩形, 图像) 受益最大, 因为 write-back 策略可以隐藏大部分 PSRAM 写延迟. 但 64B 缓存行也会引入 partial write penalty: 写零散像素到 PSRAM 时, 如果该行不在 cache 中, CPU 必须先回读整行 64B, 修改其中 2B, 再写回.
- **代价**: 可用 SRAM heap 减少 48KB. 对 PSRAM 方案 (方案3-5), 这意味着用于 SPI 中转的 DMA heap 更紧张, 可能加剧 ESP_ERR_NO_MEM 风险.

这些配置对 benchmark 数据的具体影响:
- 连续大块场景 (Single rectangle, Multiple RGB images, Multiple rectangles) 命中率高, 各方案差异小
- 文本混叠场景 (Screen sized text) 产生大量随机 PSRAM 字型访问, cache 命中率低, 与缓冲大小的关系不大
- PSRAM 方案同时承受 cache 占用 SRAM 和 SPI 中转 malloc 的双重压力

### 其他性能相关配置

| 配置 | 值 | 作用 |
|------|:--:|------|
| `CONFIG_LV_DEF_REFR_PERIOD` | 15ms (~66 FPS) | LVGL 帧率上限. 默认 30ms (33 FPS), 15ms 是表格中简单场景始终卡在 66 FPS 的原因 |
| `CONFIG_COMPILER_OPTIMIZATION_PERF` | 未启用 (使用默认 -Os) | 同上 — 其他 test_app 也默认 -Os |
| `CONFIG_FREERTOS_HZ` | 1000 | 1ms Tick (默认 100Hz/10ms). 提高任务调度精度, 但增加上下文切换开销, 影响 CPU% 数值 |
| `CONFIG_LV_USE_CLIB_MALLOC` / `STRING` / `SPRINTF` | y | LVGL 使用 C 标准库实现而非内置. 标准库 malloc 对 PSRAM 支持更好, memcpy 有优化实现 |
| `CONFIG_LV_OBJ_STYLE_CACHE` | y | 缓存对象样式计算结果, 减少重复计算 |
| `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM` | y | 把 LVGL 快速内存区放到 IRAM, 减少指令取指对数据缓存的竞争 |
| `CONFIG_LV_USE_SYSMON` / `PERF_MONITOR` | y | 性能监控覆盖层, 本 benchmark 的数据来源 |

## 逐场景对比

| 场景 | 方案1 SRAM 1/8 | **方案2 SRAM 1/4** | 方案3 PSRAM 1/8 | 方案4 PSRAM 1/4 | 方案5 PSRAM 1/2 |
|------|:---:|:---:|:---:|:---:|:---:|
| | FPS / r+f(us) | **FPS / r+f(us)** | FPS / r+f(us) | FPS / r+f(us) | FPS / r+f(us) |
| Empty screen | 52 / 4+10 | **59 / 3+10** | 52 / 5+10 | 57 / 5+9 | 58 / 5+8 |
| Moving wallpaper | 57 / 7+8 | **63 / 5+7** | 54 / 8+8 | 58 / 8+6 | 60 / 9+4 |
| Single rectangle | 66 / 0+1 | **66 / 0+1** | 66 / 1+0 | 66 / 0+1 | 66 / 0+1 |
| Multiple rectangles | 66 / 2+4 | **66 / 3+3** | 66 / 2+4 | 66 / 2+3 | 66 / 2+3 |
| Multiple RGB images | 66 / 2+1 | **66 / 1+0** | 66 / 2+1 | 66 / 1+0 | 66 / 1+0 |
| Multiple ARGB images | 66 / 3+2 | **66 / 3+0** | 66 / 3+1 | 66 / 3+0 | 66 / 3+0 |
| Rotated ARGB images | 59 / 13+1 | 56 / 12+2 | 58 / 13+1 | 54 / 13+2 | 62 / 13+0 |
| Multiple labels | 66 / 3+1 | **66 / 4+1** | 66 / 4+0 | 66 / 4+0 | 66 / 4+0 |
| Screen sized text | 20 / 47+0 | **30 / 30+0** | 20 / 48+0 | 28 / 32+0 | 33 / 25+2 |
| Multiple arcs | 66 / 1+0 | **66 / 1+0** | 66 / 0+0 | 66 / 1+0 | 66 / 1+0 |
| Containers | 65 / 8+1 | **66 / 6+1** | 66 / 8+1 | 66 / 7+1 | 65 / 6+0 |
| Containers w/ overlay | 43 / 15+4 | **51 / 14+2** | 41 / 16+4 | 45 / 16+2 | 45 / 16+2 |
| Containers w/ opa | 65 / 9+1 | **65 / 9+0** | 66 / 10+1 | 65 / 9+0 | 65 / 9+0 |
| Containers w/ opa_layer | 61 / 15+0 | **61 / 15+0** | 60 / 14+1 | 57 / 17+0 | 59 / 15+0 |
| Containers w/ scrolling | 46 / 15+3 | **54 / 12+3** | 44 / 16+3 | 49 / 14+2 | 50 / 14+2 |
| Widgets demo | 31 / 14+0 | **33 / 11+1** | 30 / 14+1 | 31 / 12+1 | 28 / 12+4 |
| | | | | | |
| **All scenes avg** | 55 / 9+2 | **58 / 8+1** | 55 / 10+2 | 56 / 9+1 | 57 / 8+1 |
| CPU | 55% | **53%** | 56% | 55% | 54% |
| Avg time | 11ms | **9ms** | 12ms | 10ms | 9ms |

```csv
Name, S1 SRAM1/8, S2 SRAM1/4, S3 PSRAM1/8, S4 PSRAM1/4, S5 PSRAM1/2
Empty screen, 52/4+10, 59/3+10, 52/5+10, 57/5+9, 58/5+8
Moving wallpaper, 57/7+8, 63/5+7, 54/8+8, 58/8+6, 60/9+4
Single rectangle, 66/0+1, 66/0+1, 66/1+0, 66/0+1, 66/0+1
Multiple rectangles, 66/2+4, 66/3+3, 66/2+4, 66/2+3, 66/2+3
Multiple RGB images, 66/2+1, 66/1+0, 66/2+1, 66/1+0, 66/1+0
Multiple ARGB images, 66/3+2, 66/3+0, 66/3+1, 66/3+0, 66/3+0
Rotated ARGB images, 59/13+1, 56/12+2, 58/13+1, 54/13+2, 62/13+0
Multiple labels, 66/3+1, 66/4+1, 66/4+0, 66/4+0, 66/4+0
Screen sized text, 20/47+0, 30/30+0, 20/48+0, 28/32+0, 33/25+2
Multiple arcs, 66/1+0, 66/1+0, 66/0+0, 66/1+0, 66/1+0
Containers, 65/8+1, 66/6+1, 66/8+1, 66/7+1, 65/6+0
Containers w/ overlay, 43/15+4, 51/14+2, 41/16+4, 45/16+2, 45/16+2
Containers w/ opa, 65/9+1, 65/9+0, 66/10+1, 65/9+0, 65/9+0
Containers w/ opa_layer, 61/15+0, 61/15+0, 60/14+1, 57/17+0, 59/15+0
Containers w/ scrolling, 46/15+3, 54/12+3, 44/16+3, 49/14+2, 50/14+2
Widgets demo, 31/14+0, 33/11+1, 30/14+1, 31/12+1, 28/12+4
```

## 结论

**方案2 (SRAM DMA, 1/4 屏)** 综合最优:

- All scenes avg FPS = 58 (最高)
- CPU 占用 53% (最低)
- Avg time = 9ms (与方案5并列最低)
- 16 个场景中有 11 个 FPS 最高或持平
- 每缓冲 38,400 字节, 恰好 1 个 SPI DMA chunk, 无分段问题

SRAM 比 PSRAM 同缓冲大小时性能更好 (方案2 vs 方案4), 因为 PSRAM 缓冲的 SPI 传输需要内部 SRAM malloc + memcpy 中转, 而 SRAM DMA 缓冲可以直接被 SPI 控制器访问.

## Appendix: PSRAM 全屏双缓冲 SPI 传输失败

### 现象

方案2 测试过程中曾尝试 PSRAM 全屏双缓冲 (方案2 之前的探索, 每缓冲 153,600 字节, DIVISOR=1), 日志中出现大量 SPI 传输错误:

```
E (10172) st7789: SPI transfer (chunk=32768, ret=-259) — ESP_ERR_NO_MEM
E (10173) st7789: SPI transfer error: ESP_ERR_NO_MEM (0x103)
```

画面可见撕裂/闪烁, 但系统未崩溃, LVGL 继续运行.

### 根因

ESP32-S3 硬件限制: **SPI DMA 控制器不能直接访问 PSRAM 地址空间**.

`esp_driver_spi` 的 `spi_device_queue_trans()` 内部 (`spi_master.c` -> `setup_priv_desc`) 会检查每个 transfer buffer 的地址是否 `esp_ptr_dma_capable()`. 对于 PSRAM 地址 (0x7xxx_xxxx 范围), 该检查返回 false, 驱动自动执行以下操作:

1. 将 PSRAM buffer 按 32KB 分块 (`SPI_LL_DMA_MAX_BIT_LEN = 262144 bits = 32KB`)
2. 为每块在 SRAM DMA 区域 `malloc()` 一个临时 buffer
3. `memcpy()` 数据从 PSRAM 到 SRAM 临时 buffer
4. 将临时 buffer 提交给 SPI DMA
5. 传输完成后 `free()` 临时 buffer

对于 320x240 RGB565 全屏双缓冲 (每缓冲 153,600 字节):
- 每次 flush 需要传输 5 个 chunk (153,600 / 32,768 = 5)
- 每个 chunk 触发一次 malloc + memcpy + free
- 在 60 FPS 下: 5 chunks x 60 FPS = 300 次 malloc/free 每秒
- SRAM DMA 区域仅约 264KB, 且与代码/栈/FreeRTOS 共享
- 高频的 32KB 分配回收导致 SRAM DMA 堆碎片化, `heap_caps_aligned_alloc(MALLOC_CAP_DMA)` 最终返回 `ESP_ERR_NO_MEM`

### 缓解尝试

| 参数 | 调整值 | 效果 |
|:----|:------|:-----|
| `trans_queue_depth` | 10 -> 4 | FPS=924 (仍有错误) |
| `trans_queue_depth` | 10 -> 2 | FPS=870 (仍有错误) |

降低队列深度反而降低 FPS, 且无法根治问题, 因为根因是 SRAM DMA 堆的分配压力, 而非队列积压.

### 推荐

对于 320x240 分辨率, **不要使用 PSRAM 全屏双缓冲**. 使用 SRAM DMA 缓冲 + height/4 或更小分片可以避免该问题.

对于更高分辨率显示 (> 480x320), 即使使用 PSRAM 分片缓冲也应谨慎评估 SPI chunk 数量, 避免超过 SRAM DMA 堆的承受能力.

---

# AuraS3 Benchmark

测试日期: 2026-05-10
测试硬件: AuraS3, CO5300 AMOLED 466x466 RGB565, QSPI 80MHz
LVGL 版本: 9.5.0
测试程序: lv_demo_benchmark (通过 test_app/ui)
串口: CH340 at /dev/ttyUSB0

## 背景

AuraS3 使用 CO5300 AMOLED, 466x466 分辨率 RGB565 (2 bytes/pixel), 一帧 434,312 字节, 是 DoerS3 (320x240) 的 5.6 倍. 接口为 QSPI (4 条数据线), 理论带宽约 4 倍于 ST7789 的 1-bit SPI.

SPI 硬件 chunk 限制 32KB 不变, 因此一帧全屏传输需要 14 个 chunk.

## 测试方案

6 种方案, 按 SRAM 在前 PSRAM 在后、行数从小到大排列.

| 方案 | 内存区域 | DIVISOR | 每缓冲大小 | 双缓冲合计 | SPI chunks |
|:----:|:--------:|:------:|:----------:|:----------:|:----------:|
| A1 | SRAM DMA | 10 (47 lines) | 43,804 B | 87,608 B | 2 |
| A2 | SRAM DMA | 8 (59 lines) | 54,988 B | 109,976 B | 2 |
| A3 | PSRAM | 8 (59 lines) | 54,988 B | 109,976 B | 2 |
| A4 | PSRAM | 4 (117 lines) | 109,044 B | 218,088 B | 4 |
| A5 | PSRAM | 2 (233 lines) | 217,156 B | 434,312 B | 7 |
| A6 | PSRAM | 1 (466 lines) | 434,312 B | 868,624 B | 14 (SPI ERR) |

> SRAM DMA: `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`
> PSRAM: `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`

## 逐场景对比

| 场景 | A1 SRAM 1/10 | A2 SRAM 1/8 | A2-Og SRAM 1/8 [^2] | A2-nocache SRAM 1/8 [^3] | A3 PSRAM 1/8 | A4 PSRAM 1/4 | A5 PSRAM 1/2 | A6 PSRAM 1/1 [^1] |
| ------ | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
|  | FPS/r+f | FPS/r+f | FPS/r+f | FPS/r+f | FPS/r+f | FPS/r+f | FPS/r+f | FPS/r+f |
| Empty screen | **31** / 4+24 | **31** / 4+23 | **31** / 4+25 | **31** / 4+23 | 29 / 12+16 | 22 / 12+27 | 21 / 11+29 | 21 / 12+29 |
| Moving wallpaper | **32** / 9+19 | **32** / 8+21 | 31 / 9+20 | **32** / 9+20 | 29 / 17+14 | 19 / 23+26 | 18 / 24+29 | 18 / 24+29 |
| Single rectangle | **66** / 0+1 | **66** / 0+1 | **66** / 1+0 | **66** / 0+0 | 65 / 0+1 | **66** / 0+1 | **66** / 1+0 | **66** / 0+1 |
| Multiple rectangles | **65** / 1+11 | **65** / 3+8 | **65** / 3+8 | **65** / 3+8 | 64 / 4+8 | 64 / 4+8 | 64 / 3+10 | 64 / 4+8 |
| Multiple RGB images | **66** / 1+0 | **66** / 1+0 | **66** / 1+0 | **66** / 1+0 | 65 / 1+0 | **66** / 1+0 | **66** / 1+1 | **66** / 1+0 |
| Multiple ARGB images | **66** / 4+0 | **66** / 4+0 | 65 / 5+0 | 65 / 5+0 | **66** / 6+0 | 65 / 6+0 | **66** / 6+0 | **66** / 6+0 |
| Rotated ARGB images | **65** / 11+1 | **65** / 11+1 | **65** / 13+0 | **65** / 13+0 | **65** / 12+0 | **65** / 12+0 | 64 / 12+0 | **65** / 12+0 |
| Multiple labels | **65** / 11+0 | **65** / 11+0 | 61 / 8+5 | 61 / 8+5 | **65** / 12+0 | **65** / 12+0 | **65** / 13+0 | **65** / 12+0 |
| Screen sized text | 11 / 79+4 | **13** / 68+4 | 11 / 81+5 | 11 / 81+5 | 11 / 78+6 | 11 / 59+24 | 11 / 51+26 | 13 / 41+29 |
| Multiple arcs | **66** / 1+0 | 65 / 0+0 | 65 / 1+0 | 65 / 1+0 | **66** / 1+0 | **66** / 1+0 | **66** / 1+0 | 65 / 1+0 |
| Containers | **66** / 5+1 | 65 / 5+3 | 65 / 6+2 | 65 / 6+2 | **66** / 6+2 | 65 / 6+2 | **66** / 6+2 | 65 / 6+2 |
| Containers w/ overlay | **26** / 22+12 | **26** / 21+12 | 24 / 25+11 | 24 / 25+11 | 24 / 29+9 | 19 / 26+21 | 19 / 28+24 | 17 / 28+24 |
| Containers w/ opa | **66** / 7+0 | **66** / 7+2 | **66** / 8+2 | **66** / 8+2 | 65 / 9+2 | 65 / 8+2 | 65 / 9+2 | 65 / 8+2 |
| Containers w/ opa_layer | 62 / 14+0 | **64** / 12+1 | 59 / 16+0 | 59 / 16+0 | 61 / 14+1 | 59 / 15+2 | 58 / 15+2 | 59 / 15+2 |
| Containers w/ scrolling | **28** / 23+9 | **28** / 22+9 | 26 / 26+9 | 26 / 26+9 | 23 / 30+9 | 17 / 29+25 | 16 / 30+28 | 16 / 29+29 |
| Widgets demo | 16 / 38+4 | **17** / 34+4 | 14 / 40+4 | 14 / 40+4 | 15 / 40+5 | 12 / 39+18 | 12 / 36+21 | 12 / 35+22 |
|  |  |  |  |  |  |  |  |
| **All scenes avg** | 49 / 14+5 | **50 / 13+5** | 48 / 15+5 | 49 / 14+5 | 48 / 16+4 | 46 / 15+9 | 46 / 15+10 | 46 / 14+11 |
| CPU | **60%** | **60%** | 64% | 62% | 63% | 63% | 63% | 63% |
| Avg time | 19ms | **18ms** | 20ms | 19ms | 20ms | 24ms | 25ms | 25ms |
| SRAM free after alloc | 111,139 | 90,659 | 125,051 | 197,427 | 201,259 | 201,259 | 201,259 | 201,259 |
| DMA free after alloc | 103,383 | 82,903 | 117,295 | 189,671 | 193,503 | 193,503 | 193,503 | 193,503 |

 [^1]: A6 (PSRAM 1/1) 出现 SPI 传输失败, 见下方附录.
 [^2]: A2-Og 与 A2 缓冲配置完全相同 (SRAM DMA 1/8), 仅编译器优化从 `-O2` 切换为 `-Og` (Debug 优化). 总 heap 增大 ~34KB, 但 render 慢约 15%, FPS 50→48, avg time 18→20ms.

 [^3]: A2-nocache 与 A2 缓冲配置完全相同 (SRAM DMA 1/8), 仅关闭 `CONFIG_LV_OBJ_STYLE_CACHE` 和 `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM`. SRAM free 从 90KB 提升到 197KB, 但性能基本不变 (FPS 50→49, avg time 18→19ms).

## 分析

AuraS3 与 DoerS3 的结论完全不同:

1. **SRAM DMA 1/8 是最优方案.** A2 (SRAM 1/8) All scenes FPS=50, avg time=18ms, 领先所有其他方案. 缩小缓冲到 1/10 (A1) FPS 降至 49, 再缩小到 1/16 虽然释放了更多 SRAM (144KB free), 但 avg time 膨胀到 22ms, render 从 13us 升到 17us. LVGL 在 1/16 方案主动报出警告: "The draw buffer's size is smaller than 1/10 screen size making rendering slower". 1/8 是 SRAM 分片的最佳平衡点.

2. **PSRAM 方案越大性能越差.** 从 A3 到 A6, 缓冲越大 FPS 越低、flush 越慢. 这与直觉相反, 原因在 AuraS3 466x466 的高像素量:
   - SRAM 1/16: 1 chunk, 直通 DMA, flush=5us avg
   - SRAM 1/10 ~ 1/8: 2 chunks, 直通 DMA, flush=5us avg
   - PSRAM 1/8: 2 chunks, 内部 memcpy, flush=4us avg (差别不大)
   - PSRAM 1/4: 4 chunks, flush=9us (翻倍)
   - PSRAM 1/2: 7 chunks, flush=10us
   - PSRAM 1/1: 14 chunks, flush=11us (SRAM 堆充足时无 SPI 错误, 但性能仍是所有方案中最差的)

3. **Buffer 过小降低渲染效率.** 1/16 的 Screen sized text render 从 68us 暴涨到 101us, Widgets demo render 从 34us 到 47us. 因为 LVGL 在局部缓冲下的渲染需要更多往返, 每趟渲染+刷屏的开销无法被更小的缓冲抵消.

4. **flush 成为瓶颈.** DoerS3 上 flush 仅 1-2us, 可忽略; AuraS3 上 flush 高达 4-11us, 且大量集中在 Empty screen / Moving wallpaper / Screen sized text 等整屏刷新的场景. Empty screen 的 flush=20-29us, 远高于其他场景, 说明全屏 dirty 时 SPI 传输时间占了主线程时间.

5. **PSRAM 方案 render 更高.** PSRAM 上 render time 普遍比 SRAM 高 30-50%, 因为 LVGL renderer 也需要读 PSRAM 数据进行 blend/composite 操作.

6. **SRAM 很紧张但够用.** A2 分配后 SRAM free=90KB, DMA free=82KB, 仍然稳定运行. PSRAM 方案释放了 110KB SRAM, 但对性能没有带来好处. 在关闭 `CONFIG_LV_OBJ_STYLE_CACHE` 和 `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM` 后, SRAM free 提升到 308KB, PSRAM 全屏 (A6) 的 SPI 中转 malloc 不再失败, 但 flush 仍高 (11us), FPS=46, avg time=25ms 与之前一致 — 性能瓶颈在 chunk 数膨胀而非内存不足.

7. **编译器优化影响 render 但不影响排名.** A2-Og 列展示了与 A2 相同缓冲配置但使用 `-Og` (Debug 优化) 的结果. 编译器的 `-O2` 内联和循环展开使 render 约快 15% (13us vs 15us), 但 flush 不受影响 (均为 5us). 所有方案的相对排名不变.

## 结论

AuraS3 (466x466 AMOLED QSPI) 上, **SRAM DMA 1/8 是最优且唯一推荐方案**:

- All scenes avg FPS=50, avg time=18ms, CPU=60%
- 缓冲放大到 PSRAM 方案性能反而下降 (chunk 数膨胀导致 flush 时间变长)
- 缓冲缩小到 SRAM 1/10 以下 render 效率降低 (LVGL 发出 buffer 过小警告)
- PSRAM 全屏在 SRAM 堆不足时触发与 DoerS3 相同的 SPI 传输失败; 释放足够 SRAM 后可稳定运行, 但性能仍远逊于 SRAM 1/8

## Appendix: AuraS3 PSRAM 全屏双缓冲 SPI 传输失败

与 DoerS3 相同的根因: PSRAM 地址非 DMA 可达, SPI driver 内部为每 32KB chunk 分配 SRAM 临时缓冲.

AuraS3 466x466 全屏 (434,312 字节 = 14 chunks) 更严重:

```text
E (54561) lcd_panel.io.spi: panel_io_spi_tx_color(395): spi transmit (queue) color failed
E (54562) co5300_spi: panel_co5300_draw_bitmap(292): send color data failed
W (54562) bsp_display: LVGL flush failed: area=0,92 466x374 err=ESP_ERR_NO_MEM
```

14 chunks x ~50 FPS = 700 次 malloc/free 每秒. Benchmark 仍完成了所有 16 个场景 (FPS 数据与 A3 接近), 但画面可见闪烁和撕裂.

**后续验证:** 在关闭 `CONFIG_LV_OBJ_STYLE_CACHE` 和 `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM` 后 SRAM free 达到 308KB (largest block 254KB), 再次测试 PSRAM 全屏未出现任何 SPI 错误. 但性能数据与有错误时一致 (FPS=46, avg time=25ms, flush=11us), 说明错误是因 SRAM 堆碎片化触发, 而非导致性能瓶颈的根因. 性能瓶颈在 14 chunks 的 memcpy 开销本身.

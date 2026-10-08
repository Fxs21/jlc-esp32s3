# AuraS3 硬件真值表

提取方式:依据同目录原理图 (`schematic/schematic.pdf`),Waveshare 官方 ESP-IDF BSP 和 Arduino v3.3.5 `pin_config.h` 交叉对照整理;部分项目已由用户对照原理图或真机日志确认.官方 BSP 和 Arduino 示例未入库,来源登记在 `docs/hw/specs/README.md`.本文件用于 BSP 实现和审核硬件事实,示例代码只能作为交叉参考,不能覆盖原理图结论.

## 0. 审核状态

| 项目 | 状态 | 备注 |
|---|---|---|
| 原理图逐页核对 | 部分确认 | `schematic/schematic.pdf` |
| 官方 ESP-IDF BSP 对照 | 已整理 | 未入库,来源见 `docs/hw/specs/README.md` |
| Arduino pin 表对照 | 已整理 | 未入库,来源见 `docs/hw/specs/README.md` |

模块接入范围和逐模块验证状态见 §13.

## 1. 板卡身份

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 板卡 | Waveshare ESP32-S3-Touch-AMOLED-1.75 | 官方 README / wiki 链接 |
| MCU | ESP32-S3 | 官方资料 |
| Display | CO5300 AMOLED | 官方 ESP-IDF BSP include `esp_lcd_co5300.h` |
| Display 分辨率 | `466 x 466` | 官方 BSP `BSP_LCD_H_RES` / `BSP_LCD_V_RES` |
| Display 形状 | 圆形,直径 `466` | 用户确认;当前 BSP 不做圆裁切(驱动只有 2x2 dirty 对齐的 `rounder_event_cb`),安全区必须由 UI 层自己保证 |
| Display 可用区 | 内接正方形约 `330 x 330`(居中),四周各约 `68` px | 由圆形几何推出;各高度可用宽度:y=0 为 `0` px,y=25 约 `210` px,y=106 约 `391` px,中心行 `466` px,y=400 约 `325` px |
| Display 接口 | QSPI | 官方 BSP `CO5300_PANEL_BUS_QSPI_CONFIG` |
| Touch | CST9217 | 官方 BSP include `esp_lcd_touch_cst9217.h` |
| IMU | QMI8658 | 真机 `i2c_scan` 和 BSP test 已确认 |
| Audio playback codec | ES8311 | 真机 `i2c_scan` + `test/bsp.sh audio auras3` 已确认 |
| Audio record codec | ES7210 | 真机 `i2c_scan` 已确认;open path 已确认 |
| PMU | AXP2101 | 真机 `i2c_scan` 和 `pmu` 自检已确认;当前 BSP 提供状态/事件/软件关机 public API,软件关机 USB 场景断电已验证 (2026-10-08) |
| RTC | PCF85063ATL | 真机 `i2c_scan` 已确认;`bsp_rtc` 读写时间已实现 |
| IO expander | TCA9554PWR | 真机 `i2c_scan` 地址 `0x20`;P7 用于 GPS reset |
| SD card | 1-bit SDMMC | CLK=GPIO2, CMD=GPIO1, D0=GPIO3;真机确认 |
| GNSS | LC76GABMD (未贴) | 原理图预留模组位和 UART/reset 网络;板上未贴器件,无法验证 |
| Camera | 不存在 | 用户确认 |

## 2. 总线与地址

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 主 I2C SDA | `GPIO15` | 用户确认;官方 BSP `BSP_I2C_SDA`;Arduino `IIC_SDA` |
| 主 I2C SCL | `GPIO14` | 用户确认;官方 BSP `BSP_I2C_SCL`;Arduino `IIC_SCL` |
| BSP I2C port | `I2C_NUM_1` | 当前 AuraS3 BSP wrapper 配置 |
| I2C speed | `400000Hz` | 当前 AuraS3 BSP wrapper 配置 |
| Touch CST9217 地址 | `0x5A` | AuraS3 真机 `i2c_scan` 确认 |
| QMI8658 地址 | `0x6B` | AuraS3 真机 `i2c_scan` 确认;BSP fallback `0x6A` |
| ES8311 地址 | `0x18` | AuraS3 真机 `i2c_scan` 确认 |
| ES7210 地址 | 7-bit `0x40` | AuraS3 真机 `i2c_scan` 确认;Espressif 8-bit `0x80` 折算为 7-bit `0x40` |
| AXP2101 地址 | `0x34` | AuraS3 真机 `i2c_scan` 确认 |
| PCF85063 地址 | `0x51` | AuraS3 真机 `i2c_scan` 确认 |
| TCA9554PWR 地址 | `0x20` | AuraS3 真机 `i2c_scan` 确认 |
| GNSS 接口 | UART | ESP32 RX `GPIO18` <- GNSS TX,ESP32 TX `GPIO17` -> GNSS RX |
| GNSS baudrate | `38400` | 当前 BSP 实现值;板上未贴模组,无法验证 |

### 总线冲突/待裁决项

- Arduino `pin_config.h` 同时出现 `I2S_MCK_IO=16` 和实际 `MCLKPIN=42`;当前实现以官方 ESP-IDF BSP 和真机确认的 `GPIO42` 为准.
- SD wiring 官方资料偏向 SDMMC 1-bit,当前 BSP 已使用 SDMMC 1-bit 并真机确认.

### AuraS3 真机 I2C scan 结果

| 7-bit 地址 | 器件 | 状态 | 备注 |
|---:|---|---|---|
| `0x18` | ES8311 | 已确认 | playback codec |
| `0x20` | TCA9554PWR | 已确认 | 8-bit IO expander |
| `0x34` | AXP2101 | 已确认 | PMU / charger |
| `0x40` | ES7210 | 已确认 | record ADC / mic codec |
| `0x51` | PCF85063 | 已确认 | RTC;不要与 CO5300 brightness command `0x51` 混淆 |
| `0x5A` | CST9217 | 已确认 | touch controller |
| `0x6B` | QMI8658 | 已确认 | IMU;WHOAMI register `0x00` 期望 `0x05` |

## 2.1. IO expander, TCA9554PWR

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 芯片 | TCA9554PWR | 用户确认;对应 I2C scan `0x20` |
| I2C 地址 | `0x20` | TCA9554 地址由 A2/A1/A0 决定;`0x20` 表示 A2/A1/A0 应为低 |
| IO 数量 | 8-bit `P0..P7` | P0..P2 未连接;P3..P7 已由用户确认网络 |

### TCA9554 IO 分配

| TCA9554 IO | 连接网络/用途 | 状态 | 备注 |
|---|---|---|---|
| P0 | NC | 用户确认 | 未连接 |
| P1 | NC | 用户确认 | 未连接 |
| P2 | NC | 用户确认 | 未连接 |
| P3 | RTC_INT | 用户确认 | PCF85063 interrupt/clock alarm 类输出,配置为 input |
| P4 | SYS_OUT / EXIO4 | 用户确认/原理图复核 | 由 KEY2/PWRON 通过 NMOS 生成的按键/电源状态信号,配置为 input |
| P5 | AXP_IRQ | 用户确认 | AXP2101 中断输出,配置为 input |
| P6 | QMI_INT | 用户确认 | QMI8658 中断输出,配置为 input |
| P7 | GPS_RST | 用户确认 | GNSS reset 控制,当前 BSP 释放 reset 后再打开 UART |

### SYS_OUT / KEY2 关系

- 原理图中 `KEY2`,`PWRON` 与 `SYS_OUT` 通过 NMOS 管相连: `PWRON` 经 `RP6 510R` 到控制节点,`KEY2` 按键将该节点拉到 GND,该节点再经 `R12 1K` 驱动 NMOS `T1`;`SYS_OUT` 由 `R9 10K` 上拉到 `VCC3V3`,并可被 `T1` 拉低.
- 因此 `SYS_OUT / EXIO4` 不是普通电源输出控制脚,更像一个由 `KEY2/PWRON` 组合生成的电源键/系统状态输入信号;TCA9554 侧应按 input 读取.
- 真机已确认极性: `KEY2` 松开 / idle 时 `SYS_OUT=0`;按下 `KEY2` 时 `SYS_OUT=1`.这是 board internal/raw debug 结论,不进入稳定 public PMU status.

## 3. Display, CO5300 AMOLED QSPI

| 信号 | ESP32-S3 GPIO | 来源/备注 |
|---|---:|---|
| LCD_CS | `GPIO12` | 官方 BSP `BSP_LCD_CS`;Arduino `LCD_CS` |
| LCD_PCLK / SCLK | `GPIO38` | 官方 BSP `BSP_LCD_PCLK`;Arduino `LCD_SCLK` |
| LCD_DATA0 / SDIO0 | `GPIO4` | 官方 BSP `BSP_LCD_DATA0`;Arduino `LCD_SDIO0` |
| LCD_DATA1 / SDIO1 | `GPIO5` | 官方 BSP `BSP_LCD_DATA1`;Arduino `LCD_SDIO1` |
| LCD_DATA2 / SDIO2 | `GPIO6` | 官方 BSP `BSP_LCD_DATA2`;Arduino `LCD_SDIO2` |
| LCD_DATA3 / SDIO3 | `GPIO7` | 官方 BSP `BSP_LCD_DATA3`;Arduino `LCD_SDIO3` |
| LCD_RST | `GPIO39` | 官方 BSP `BSP_LCD_RST`;Arduino `LCD_RESET` |
| LCD_TE | `GPIO13` | 用户确认;当前保留硬件事实,默认不启用 TE wait |
| LCD_BACKLIGHT GPIO | `GPIO_NUM_NC` | AMOLED 不使用独立 PWM 背光 |
| SPI host | `SPI2_HOST` | 当前 BSP display 使用 |
| 色彩格式 | RGB565,16bpp | native stream 为 high-byte-first RGB565;人工项 color-sweep 真机确认 (2026-09-24, sha `57e98bd`) |
| 分辨率 | `466 x 466` | 官方 BSP / Arduino pin 表 |
| gap/offset | x gap `0x06`,y gap `0` | 当前 BSP 设置;人工项 edge-marker 确认无整列偏移 (2026-09-24, sha `57e98bd`) |
| LVGL dirty alignment | `2 x 2` | 当前 BSP rounder 保证 x/y 偶数起点和奇数终点 |

### Display 初始化要点

- 当前 BSP 使用官方 `esp_lcd_co5300` QSPI panel driver,panel 生命周期在 `auras3/display.c` 中统一管理,backlight 和 display 通过 ref_count 共享.
- display 自检真机通过 (2026-09-24, sha `57e98bd`, `test/bsp.sh display auras3`, 7/7 PASS): 覆盖 desc,open/close/重开,参数校验,传输完成回调计数,以及 color-sweep,frame-centered,edge-marker 三个人工项;期间修复 close 未归零 ref_count 导致重开时 panel 为空的缺陷.
- `bsp_ui` 自检真机通过 (2026-09-30, sha `0430fbb`, `test/bsp.sh ui auras3`, 6/6 PASS, 人工项 widgets-demo yes): 覆盖 LVGL display/indev/背光绑定,handle 独占,参数校验,`process()` 延时提示和刷帧计数;画面来源是 LVGL 内置 widgets demo,人工确认画面完整且触摸跟手;屏载 perf monitor 读数: 静态画面 60 FPS / CPU 3%,复杂页面滑动最低 17 FPS / CPU 98%.
- CO5300 init table 已按厂家 QSPI/RGB565 序列收敛: `FE 00`,`C4 80`,`3A 55`,`35 00`,`53 20`,`51 00`,`63 FF`,`2A 00 06 01 D7`,`2B 00 00 01 D1`,`11` delay `60ms`,`29`.
- 厂家序列使用 `51 FF` 直接满亮;当前 BSP 保留 `51 00`,避免 init 阶段亮脏首帧,由 UI/backlight API 后续设置亮度.
- 每次 open 都会做硬件 reset: 驱动把 RST 拉低 `10 ms`, 拉高后再等 `150 ms` 才发第一条命令, 亮度 `51 00` 排在 6 条命令之后. 亮度 0 的黑屏上反复 open/close 能看见每次一次亮闪(2026-09-29, `test/ui` 观察); 成因 (复位后的默认亮度或 GRAM 残留) 未逐帧确认, 测试侧先按"ui 全程只 open 一次"规避.
- `bsp_display` public API 只提供 native async transfer + wait,不提供 `fill` 或 public host-order writer.
- AuraS3 native display contract 是 high-byte-first RGB565 byte stream;LVGL flush 必须执行 `lv_draw_sw_rgb565_swap()`.
- CO5300 QSPI 局部刷新区域需要 2 像素对齐: invalid area 的 `x1/y1` 向下取偶数,`x2/y2` 向上扩到奇数;未对齐时 LVGL demo 动态区域会出现残留.
- 该残留不是 AMOLED 物理残影,也不是 RGB565 endian 问题;它属于 CO5300/QSPI partial refresh 窗口约束.
- LVGL render mode 为 PARTIAL;double buffer,lines=59,单 buffer 54988 bytes,优先 SRAM DMA.
- TE wait 默认不启用,`GPIO13` 只作为硬件事实保留;实验记录见 `docs/hw/auras3-display-te.md`.

## 4. Brightness / Backlight

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 背光类型 | AMOLED 内部亮度控制 | 无独立 LEDC 背光 GPIO |
| 控制方式 | CO5300 command `0x51` | 官方 `bsp_display_brightness_set()` 同路径 |
| panel raw 范围 | `0..255` | CO5300 brightness command 参数 |
| BSP public 范围 | `0..100%` | 当前 BSP 映射到 `0..255` |
| 默认亮度 | `0` | 避免启动阶段绿屏/脏首帧;UI 首帧后再开亮度 |

### Brightness 注意事项

- AuraS3 不复用 DoerS3 LEDC backlight driver.
- `bsp_backlight_open()` 是 AMOLED brightness proxy,不在 public API 泄露 CO5300 细节.
- `bsp_ui_set_backlight(ui, percent)` 是 app UI 推荐入口.
- 亮度映射真机确认 (2026-09-24, sha `129b1ca`, `test/bsp.sh backlight auras3`, 6/6 PASS): 100/50/10/0% 四档目视逐档变暗,0% 接近全黑,0% -> 100% 能恢复;实测硬件值 10% -> `25`,50% -> `127`,100% -> `255`;`set_percent(>100)` 收敛到 100.

## 5. Touch, CST9217

| 信号 | ESP32-S3 GPIO / 地址 | 来源/备注 |
|---|---:|---|
| I2C SDA | `GPIO15` | 官方 BSP / Arduino pin 表 |
| I2C SCL | `GPIO14` | 官方 BSP / Arduino pin 表 |
| TOUCH_RST | `GPIO40` | 官方 BSP `BSP_LCD_TOUCH_RST`;Arduino `TP_RESET` |
| TOUCH_INT | `GPIO11` | 官方 BSP `BSP_LCD_TOUCH_INT`;Arduino `TP_INT` |
| I2C 地址 | `0x5A` | 真机 `i2c_scan` 确认 |
| 坐标范围 | `466 x 466` | 官方 BSP touch config |
| mirror_x | `1` | 当前 BSP 对齐官方 |
| mirror_y | `1` | 当前 BSP 对齐官方 |
| swap_xy | `0` | 当前 BSP 对齐官方 |

## 6. SD card

| 信号 | ESP32-S3 GPIO | 当前 BSP 用途 | 来源/备注 |
|---|---:|---|---|
| SD_CLK | `GPIO2` | SDMMC CLK | 官方 BSP `BSP_SD_CLK`;Arduino `SDMMC_CLK` |
| SD_CMD | `GPIO1` | SDMMC CMD | 官方 BSP `BSP_SD_CMD`;Arduino `SDMMC_CMD` |
| SD_D0 | `GPIO3` | SDMMC DAT0 | 官方 BSP `BSP_SD_D0`;Arduino `SDMMC_DATA` |
| Mount point | `/sdcard` | FAT mount | `test/sdcard` 真机确认 |

### SD 注意事项

- SDMMC 1-bit 模式真机验证通过: `test/sdcard` 自检 mount,读写正常,与官方 BSP 行为一致.
- 此前使用过 SDSPI(SPI3_HOST + GPIO41 CS),真机 mount 31GB SDHC 卡成功.切换到 SDMMC 后,GPIO41 不再用于 SD card.
- SDIO 探测阶段 cmd=52 / cmd=5 command not supported 日志属于正常输出,只要 `mounted: 1` 即为成功.

## 7. IMU, QMI8658

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 芯片 | QMI8658 | 真机 test 已确认 |
| 总线 | 主 I2C `GPIO15` / `GPIO14` | 当前 BSP shared I2C wrapper |
| 地址 | `0x6B` | AuraS3 真机 `i2c_scan` 确认;`0x6A` 作为 fallback |
| 默认能力 | accel + gyro + temperature | `imu` test 已确认读数 |
| 中断脚 | TCA9554 P6 `QMI_INT` | 当前 BSP polling,未使用中断 |

### IMU 验证结果

代表性真机日志:

```text
QMI8658 initialized successfully
accel m/s2: x= 1.774 y=-0.077 z=-9.917
gyro  rad/s: x=-0.059 y= 0.008 z= 0.114
temp      C: 33.71
```

## 8. Audio, ES8311 + ES7210 + I2S

| 信号 | ESP32-S3 GPIO | 连接/用途 | 来源/备注 |
|---|---:|---|---|
| I2C SDA | `GPIO15` | codec control | 官方 BSP / 真机 scan |
| I2C SCL | `GPIO14` | codec control | 官方 BSP / 真机 scan |
| I2S_BCLK / SCLK | `GPIO9` | bit clock | 官方 BSP `BSP_I2S_SCLK`;Arduino `BCLKPIN` |
| I2S_WS / LCLK | `GPIO45` | word select | 官方 BSP `BSP_I2S_LCLK`;Arduino `WSPIN` |
| I2S_DOUT | `GPIO8` | ESP32 -> ES8311 playback data | 官方 BSP `BSP_I2S_DOUT` |
| I2S_DIN | `GPIO10` | ES7210 record data -> ESP32 | 官方 BSP `BSP_I2S_DSIN` |
| I2S_MCLK | `GPIO42` | codec master clock | 用户确认;官方 BSP `BSP_I2S_MCLK` |
| PA enable | `GPIO46` | NS4150B `CTRL` | 原理图确认 `NS4150B`;官方 BSP `BSP_POWER_AMP_IO`;direct GPIO |

### Audio codec 角色

| 芯片 | 地址 | 角色 | 来源/备注 |
|---|---:|---|---|
| ES8311 | `0x18` | playback / DAC | 真机 `i2c_scan` + `test/bsp.sh audio auras3` 确认 |
| ES7210 | 7-bit `0x40` | record / ADC | 真机 `i2c_scan` + 自检录音路径确认 |

### 模拟音频路径

| 路径 | 连接 | 来源/备注 |
|---|---|---|
| 播放 | ESP32-S3 `GPIO8` -> ES8311 `DSDIN` -> `OUTP/OUTN` -> NS4150B -> speaker | 真机确认 |
| 板载双麦 | MIC1/MIC2 -> ES7210 `MIC1P/MIC1N`,`MIC2P/MIC2N` | 真机确认 (双麦拾音用例) |
| 播放回采 | ES8311 `OUTP/OUTN` -> ES7210 `MIC3P/MIC3N` | 真机确认: 播放 1 kHz 时 TDM slot 1 有该音 (2026-10-08, sha `5b2bc27`) |
| 未使用 | ES8311 `ASDOUT` 未接;ES8311 `MIC1P/MIC1N/MICBIAS` 未用 | 不应设计为 ES8311 ADC 录音 |

### Audio 注意事项

- BSP 支持三种录音 mask: `0` (纯播放),`MIC1|MIC2` (16-bit stereo),`MIC1|MIC2|LOOPBACK` (TDM 3 通道);通道顺序 `[MIC1, 回采, MIC2]`,回采在 slot 1;语义见 `docs/bsp/audio.md`.
- ES7210 `MIC3` 是 ES8311 模拟输出的回采,不是第三个板载麦克风;回采是 line 级信号,录音增益用 `0 dB` (30 dB 会削顶).
- full-duplex 真机确认: 播放 1 kHz 时回采通道 rms 3424 / tone_ratio 0.495,主频扫描峰值 1000 Hz,同期 MIC1/MIC2 安静 (2026-10-08, sha `5b2bc27`, `test/bsp.sh audio auras3`, 10/10 PASS 1 ignored).
- 喇叭响度未验证: 台位未接喇叭,人工项 pending,接喇叭后补测.
- ES8311 private driver 对 `GPIO_REG44` 首次写失败做 retry;该寄存器是 ES8311 内部 `0x44`,不是 ESP32 GPIO44.

## 9. PMU, AXP2101

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 芯片 | AXP2101 | 示例 / datasheet / 真机 scan |
| I2C 地址 | `0x34` | AuraS3 真机 `i2c_scan` 确认 |
| I2C SDA/SCL | 主 I2C `GPIO15` / `GPIO14` | 用户确认 AXP2101 挂主 I2C;PMU 示例 defaults `8/7` 视为残留 |
| IRQ | TCA9554 P5 `AXP_IRQ` | 用户确认 |
| 能力 | 电池/充电/VBUS/温度/系统电压/KEY2 事件 | 真机 test 和 AXP2101 driver 已确认 |

### PMU 当前结论

- 当前 BSP 提供 `bsp_pmu` public API: `open` / `close` / `get_status` / `get_events` / `power_off`;`open` 固定完成 ADC, fuel gauge 和事件 IRQ 最小使能.
- open 时把 IRQ 使能显式收敛到事件表映射的位 (`REG40H`=0,`REG41H`=0xFC,`REG42H`=0x18),避免 `AXP_IRQ` 被未映射事件 (gauge SOC,电池温度,PWRON 边沿) 拉低.
- 已确认 `KEY2` 短按 / 长按 event,长按硬关机,关机后短按 `KEY2` 重新开机 (2026-10-08).
- 已确认 `SYS_OUT` idle 为 `0`,按下 `KEY2` 为 `1`;`AXP_IRQ` idle 为 `1`,pending IRQ 为 `0`.
- 已确认电池插入 / 拔出,充电开始,VBUS/电池/system voltage,PMU 温度和 `battery_percent` 读取.
- public API 不暴露 raw AXP2101 register,不开放 rail control 或充电参数配置.详细结论见 `docs/hw/auras3-pmu-key.md`.
- `bsp_pmu_power_off()` (写 `REG10H[0]`) USB 场景已验证 (2026-10-08): 断电成功, 无自动回电, 关机后短按 `KEY2` 可开机;仅电池 / USB+电池场景待电池. 验证入口是 `test/pmu` 自检结束后的引导步.

## 10. RTC, PCF85063

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 芯片 | PCF85063ATL | Arduino RTC demo / datasheet / 真机 scan |
| 总线 | 主 I2C | Arduino demo 使用 `Wire.begin(IIC_SDA,IIC_SCL)` |
| I2C 地址 | `0x51` | AuraS3 真机 `i2c_scan` 确认 |
| VDD | AXP2101 `RTCLDO`,网络 `VCC-RTC` | 原理图 page 1;RTCLDO 在 PMU 各状态下保持输出,无软件开关 |
| 备份供电 | AXP2101 `VBACKUP` 接电池侧网络 `VBAT2` (`CHG_RTC`) | 原理图 page 1;无电池时拔 USB 预期丢时,待实测 |
| interrupt | `INT` (pin 4,open-drain) -> TCA9554 P3 `EXIO3` | 原理图 page 1;TCA9554 输入自带 100 kΩ 内部上拉 |
| 到 SoC 的中断线 | 无 | TCA9554 自身 `INT` 只经 R19 10K 上拉到 VCC3V3,未接 ESP32 GPIO;事件只能轮询 |
| CLKOUT | 未连接 | 原理图 page 1,pin 9 标 n.c. |

### RTC 实现

- `bsp_rtc` public API 已实现: 读写时间 + OS 丢时标记,见 `components/bsp/include/bsp_rtc.h`.
- driver 强制 24 小时制,写秒寄存器清 OS,并按日期重算 weekday;越界时间字段拒绝写入.
- 不做 alarm / timer / CLKOUT / ppm offset / RAM byte: 没有到 SoC 的中断线,且 CLKOUT 未连接.
- 无电池时"掉电走时"无法验证;`test/rtc` 只覆盖可自动判定的部分,掉电保持留待装电池后补测.

## 11. GNSS

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 模块 | LC76GABMD (未贴) | 原理图有模组位号和 GPS reset/UART 网络,板上未贴器件;`docs/hw/specs/chips/max-m10s_datasheet.pdf` 是 DoerS3 模块资料,与本板无关 |
| 接口 | UART | 用户确认;Arduino I2C 示例不作为本板接口真值 |
| UART TX/RX | ESP32 RX `GPIO18` <- GNSS TX;ESP32 TX `GPIO17` -> GNSS RX | 用户确认 |
| baudrate | `38400` | 当前 BSP 实现;硬件未连接,待真机验证 |
| reset | TCA9554 P7 `GPS_RST` | 当前 BSP 通过 IO expander 释放 reset |

### GNSS 注意事项

- 模组未贴装,自检 `test/bsp.sh gnss auras3` 在 3 s 探测窗内无 NMEA 时输出 `SKIP`,不作为失败结论.
- 等硬件连接后,优先验证 `38400` baud,TX/RX 方向和 `GPS_RST` 极性.
- 现有 `bsp_gnss` public API 保持 UART/NMEA raw byte stream,不引入结构化定位 API.

## 12. Camera

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 是否存在 camera | 不存在 | 用户确认 |
| Sensor | N/A | 无 camera |
| Interface | N/A | 无 camera |
| XCLK/PCLK/VSYNC/HREF/D0..D7 | N/A | 无 camera |
| PWDN/RESET | N/A | 无 camera |

### Camera 实现建议

- AuraS3 不实现 camera,保持 `ESP_ERR_NOT_SUPPORTED`.
- 不要为了 shell 增加 AuraS3 camera capture 命令.

## 13. 当前 BSP 映射

| BSP 模块 | 当前状态 | 依据 |
|---|---|---|
| `bsp_board` | 已实现 | shell `bsp info` 打印 desc.present: touch,backlight,imu,gnss,sdcard,audio,pmu 为 true,camera 为 false (2026-09-30, sha `5f4674c`;rtc 行随 `f54f62a` 加入);display 无 desc,见 `bsp_display` 行 |
| `bsp_display` | 已实现 | CO5300 QSPI native async transfer,真机确认 (2026-09-24, sha `57e98bd`, `test/bsp.sh display auras3`, 7/7 PASS);UI 真机确认 |
| `bsp_ui` | 已实现 | LVGL display/indev/背光组合通路,真机确认 (2026-09-30, sha `0430fbb`, `test/bsp.sh ui auras3`, 6/6 PASS) |
| `bsp_backlight` | 已实现 | CO5300 `0x51` brightness percent mapping,真机确认 (2026-09-24, sha `129b1ca`, `test/bsp.sh backlight auras3`, 6/6 PASS) |
| `bsp_touch` | 已实现 | CST9217,真机确认 (2026-09-23, sha `99a464c`, `test/bsp.sh touch auras3`, 5/5 PASS) |
| `bsp_sdcard` | 已实现 | SDMMC 1-bit,真机确认 (2026-09-23, sha `99a464c`, `test/bsp.sh sdcard auras3`, 8/8 PASS) |
| `bsp_imu` | 已实现 | QMI8658,真机确认 (2026-09-23, sha `99a464c`, `test/bsp.sh imu auras3`, 6/6 PASS) |
| `bsp_audio` | 已实现 | 会话模型 (record mask,TDM 回采和 full-duplex),真机确认 (2026-10-08, sha `5b2bc27`, `test/bsp.sh audio auras3`, 10/10 PASS 1 ignored) |
| `bsp_gnss` | 已实现但未硬件验证 | 模组未贴装,自检 3 s 无 NMEA 输出 `SKIP` (2026-09-30, sha `5f4674c`) |
| `bsp_pmu` | 已实现 | AXP2101 status/events/power_off;真机自检 (2026-10-08, sha `90dc3f7-dirty`, `test/bsp.sh pmu auras3`, 7 项 0 失败 1 ignored);power_off USB 场景断电已验证 |
| `bsp_camera` | unsupported | 用户确认无 camera;自检输出 `SKIP` (2026-09-30, sha `5f4674c`) |
| `bsp_rtc` | 已实现 | PCF85063ATL 读写时间 + OS 标志,真机确认 (2026-09-30, sha `f54f62a`, `test/bsp.sh rtc auras3`, 5/5 PASS) |
| TCA9554PWR | 内部 helper 已接入 | 当前用于 GNSS reset 和 input default setup |

## 14. 例程对照结论

对照 `docs/code/auras3/ESP-IDF-v5.4/` 例程源码,逐项核对初始化序列,地址和取值;一致项已由 driver 覆盖,差异项按下表记录"例程行为 / 本仓库取舍 / 理由".核对基线: 2026-09-23, sha `99a464c`.已核对完毕的例程从 `docs/code/` 删除.

### 03_QMI8658 -> `bsp_imu`

一致项: 地址 `0x6B` (fallback `0x6A`),`CTRL1` bit6 寄存器地址自增,accel/gyro 12 字节 burst 小端解析,`STATUS0 & 0x03` 判数据就绪,温度通道,时间戳取 24 位采样计数.

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| accel `4G@1000Hz`,gyro `64dps@896.8Hz` | accel `8G@1000Hz`,gyro `512dps@1000Hz` | 本仓库自有取值,覆盖更大动态范围;例程取值不是硬件约束 |
| 开 accel LPF (mode 0) 和 gyro LPF (mode 3),写 `CTRL5` | `CTRL5` 保持 0 | BSP 交付未滤波的原始读数,滤波策略交给上层;不影响寄存器序列正确性 |
| `configAccelerometer` / `configGyroscope` 默认 `selfTest = true`,置 `CTRL2/CTRL3` bit7 | 不开 self-test | self-test 是产测动作,不在 `open()` 常态开启 |
| I2C 100 kHz | 400 kHz | 与全板 I2C 速率统一 |
| `CTRL1` 保持出厂 `BE=1`,手册 Table 22 写作"大端",例程与本仓库都按小端解析 | 沿用板厂实现 | 真机加速度模长 9.96~9.99 m/s^2 合理;手册与实测矛盾处留在本条,需要时用静态姿态基准复测 |

### 04_SD_MMC -> `bsp_sdcard`

一致项: `CLK=GPIO2`,`CMD=GPIO1`,`D0=GPIO3`,1-bit 总线,挂载点 `/sdcard`,`max_files=5`,20 MHz,内部上拉.

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| `format_if_mount_failed = true` (`CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED=y`) | `format_if_mount_failed = false` | 格式化是破坏性操作,BSP 不自动执行 |
| `allocation_unit_size = 16 KB` | 32 KB | 批量写场景减少 FAT 簇链开销;该值只影响性能,不影响挂载兼容性 |
| `CONFIG_FATFS_VFS_FSTAT_BLKSIZE=4096` | IDF 默认 | 当前没有对该值敏感的使用路径;需要时再对齐 |
| 挂载前把 TCA9554 `P0/P1/P2/P7` 拉低 200 ms 再拉高 | 不复制 | AuraS3 上 `P0..P2` 未连接,`P7` 是 `GPS_RST`;该序列对 SD 无影响,复制会误动 GPS 复位 |
| I2C 200 kHz (仅用于 TCA9554) | 400 kHz | 与全板 I2C 速率统一 |

### 07_Touch -> `bsp_touch`

一致项: 地址 `0x5A`,`RST=GPIO40`,`INT=GPIO11`,面板 `466x466`,`mirror_x/mirror_y=true`,`swap_xy=false`,I2C 走 `GPIO15/GPIO14`.

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| I2C 100 kHz | 400 kHz | 与全板 I2C 速率统一 |
| 30 ms 轮询读点 | INT 下沿中断置位,`read()` 按需刷新寄存器并缓存,1.5 s 无上报清缓存 | 轮询占 I2C 带宽且延迟大;缓存避免重复读寄存器 |
| `getPoint(x, y, 2)` 请求 2 点 | `max_points = 1` | 当前只承诺单点;多点能力未真机确认,见待办 |
| `setMaxCoordinates(466, 466)` | `465` | 该值是镜像轴 (`x_max - x`),取 `466 - 1` 才能保证镜像后仍落在可报坐标范围内 |

### 05_LVGL_WITH_RAM -> `bsp_ui` (QSPI 面板 + LVGL)

一致项: 面板 pin 组和 QSPI 参数 (CS `GPIO12`,PCLK `GPIO38`,D0..D3 `GPIO4..7`,RST `GPIO39`,quad mode,max transfer 整屏 `466*466*2`);11 条 init 序列和取值 (`FE 00`,`C4 80`,`3A 55`,`35 00`,`53 20`,`63 FF`,`2A 00 06 01 D7`,`2B 00 00 01 D1`);`466x466` RGB565 16bpp;LVGL rounder 的 2 像素对齐规则 (x1/y1 向下偶数,x2/y2 向上奇数).

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| `esp_lcd_sh8601` 面板驱动 | `espressif/esp_lcd_co5300` | 屏实际控制器是 CO5300;SH8601 是同族,例程沿用了通用组件 |
| init 里 `51 FF` 直接满亮 | `51 00`,首帧后由 `bsp_backlight` / `bsp_ui` 设置 | 避免上电亮脏首帧;percent 映射真机确认 (见 §4) |
| `2B` 后延迟 `600 ms`,`11` 后延迟 `600 ms` | `2B` 无延迟,`11` 后 `60 ms` | CO5300 组件默认值;display/ui 自检和 demo 真机无异常,无需更长等待 |
| draw buffer 2 x `V_RES/4` (116 行),`MALLOC_CAP_DMA` | 2 x `V_RES/8` (59 行),优先 SRAM DMA,PSRAM 兜底 | 双缓冲总量相近 (216 KB vs 110 KB),59 行能放进内部 SRAM;屏载 perf monitor 实测静态 60 FPS / 复杂滑动 17 FPS |
| LVGL 8 手写 port: esp_timer `2 ms` tick,task + mutex,app 自己注册 disp/indev | LVGL 9 + `bsp_ui` 一次 open (display/indev/backlight/tick 都在 BSP 内) | app 只调 `bsp_ui_open()` / `bsp_ui_process()`,不接触 LVGL port 细节 |
| 触摸走 SensorLib 轮询 `getPoint(x, y, 2)` | `bsp_touch` INT 中断,单点 | 同 `07_Touch` 差异表 |

### 02_PCF85063 -> `bsp_rtc`

核对基线: 2026-09-30, sha `f54f62a`.

一致项: 地址 `0x51`,主 I2C `GPIO15/GPIO14`;CTRL1 `0x00`,`0x04..0x0A` 秒..年 7 字节 burst;秒寄存器 bit7 为 OS 标志,写入时清 OS;BCD 编解码;默认 24 小时制 (CTRL1 bit1=0);初始化清 STOP (CTRL1 bit5);字段顺序和 weekday 计算 (0=Sunday) 一致;OS 报告语义一致 (例程 `available`,本仓库 `valid_out`).

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| alarm (get/set/enable/reset 和 `setAlarmBy*`),timer,CLKOUT (7 档),`stop`/`start` | 不做 | `INT` 只到 TCA9554 `EXIO3`,没有到 SoC 的中断线;`CLKOUT` 未连接 (见 §10) |
| I2C 100 kHz | 400 kHz | 与全板 I2C 速率统一 |
| init 读秒寄存器,BCD 秒 > 59 视为探测失败 | 只读 CTRL1 探测在线 | 该启发式会被总线毛刺和脏读数误判;探测只需确认芯片应答 |
| `is24Hour` 从 CTRL1 读回,非 24H 才清 bit1 | `open()` 无条件强制 24 小时制 | public API 固定 24 小时制,不暴露 12H |
| `setDateTime()` 不校验字段 | `set_time()` 校验 2000..2099 / 1..12 / 1..31 / 0..23 / 0..59 / 0..59 | 非法值写进芯片后无法区分"芯片故障"和"调用错误" |
| weekday 读回解析成 `datetime.week` | public API 不暴露 weekday | 芯片 weekday 只是用户计数器,时间语义不依赖它 |

### 06_I2SCodec -> `bsp_audio`

核对基线: 2026-09-30, sha `5b2bc27`.

一致项: 引脚组与 AuraS3 一致 (MCLK `GPIO42`,BCLK `GPIO9`,WS `GPIO45`,DO `GPIO8`,DI `GPIO10`,PA `GPIO46`,I2C SDA `GPIO15` / SCL `GPIO14`);ES8311 地址 `0x18`;ESP32-S3 作 I2S master 并输出 MCLK;16-bit 采样;默认音量 80;默认采样率 16 kHz.

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| echo 模式用 ES8311 ADC 录音 (`es8311_microphone_config` + MIC 增益),读 I2S RX 原样写回 TX | 录音走 ES7210;回采是 ES8311 模拟输出 -> ES7210 `MIC3` | 本板 ES8311 `ASDOUT` 未接,例程的 echo 数据源在 AuraS3 上不存在 |
| `EXAMPLE_MCLK_MULTIPLE = 384` | `256` | 例程注释自述 24-bit 才需要 384;本仓库固定 16-bit,256 即可 |
| app 自建连续播放 task,`i2s_channel_write` 用 `portMAX_DELAY` | `play_write` 由调用者按 chunk 驱动,带 `timeout_ms` 并返回实际字节数 | public API 不内置播放 task,节奏和缓冲策略留给 app |
| `gpio_init()` 开机把 PA 拉高并常开 | PA 在 `play_start()` 打开,`play_stop()` / `close()` 关闭 | 避免上电和静音期把 codec 底噪放到喇叭,也避免空载功耗 |
| 初始化时同时 enable TX/RX,`auto_clear = true` | 通道按需 enable/disable,TX 预载静音后再 enable | 预载保证第一次 enable 就送静音;`auto_clear` 只在 underrun 后生效 |
| 只有 ES8311,单一 16 kHz 编译期常量 | 录音依赖 ES7210 (3 通道回采需要 TDM),采样率运行时可配 8000..48000 Hz | 回采需要 3 个 ADC;codec driver 带各档系数,自检覆盖非默认采样率 |
| I2C 100 kHz | 400 kHz | 与全板 I2C 速率统一 |

### 01_AXP2101 -> `bsp_pmu`

核对基线: 2026-10-08, sha `90dc3f7-dirty`.

一致项: 地址 `0x34`;ADC 测量使能集合 (电池/USB/系统电压/温度) 和 TS pin 关闭 (板上无电池温度检测,否则影响充电);IRQ 源集合 (VBUS 插拔,电池插拔,PKEY 短按/长按,充电开始/完成) 与 `REG41H`=0xFC / `REG42H`=0x18 一致;IRQ status 读后清 latch 的消费模型;充电阶段枚举 (trickle/precharge/CC/CV/done/not charging);`isCharging`/`isDischarge`/`isStandby` 与 `BSP_PMU_POWER_STATE_*` 同源 (`REG01H`);`enableGauge()` 写 `REG18H[3]` 与本仓库一致 (例程未调用,依赖复位默认).

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| 设置预充 50 mA / 恒流 200 mA / 截止 25 mA / 目标电压 `4V1` | 不改充电参数 | 充电曲线属于电池和产品策略,未经整机验证不进 BSP;误设影响安全 |
| 打印并 (注释形式) 提供 DC1-5 / ALDO / BLDO / DLDO 开关与调压,源码自带警告"不知道外部负载电压时运行可能烧负载" | 不开放 rail control | rail 到外设映射未验证;例程自己的警告也说明误改 rail 有实际风险 |
| `sdkconfig.defaults` 用 `SCL 14` / `SDA 15` / `INT -1`;Kconfig 通用默认是 `22/21/35` | 走主 I2C `GPIO14/15`,无 GPIO 中断线 | 入库副本与板级一致;此前"总线冲突/待裁决项"记录的 `7/8` 未在入库副本出现,随删除关闭 |
| legacy `driver/i2c.h` 100 kHz | `i2c_master` 400 kHz | 与全板 I2C 速率统一 |
| GPIO 负沿中断 + queue + 任务分发 (`irq_init`,实际被注释);1 s 任务轮询 `getIrqStatus` 并打印 | 无回调无后台任务,轮询 `get_events`,latch 语义 | `AXP_IRQ` 不直接接 ESP32 GPIO,经 TCA9554 才可见;无 SoC 中断线 |
| 不演示软件关机 | `bsp_pmu_power_off()` (写 `REG10H[0]`) 已进 public API,USB 场景真机验证 | 关机能力由本仓库补充;验证记录见 `docs/hw/auras3-pmu-key.md` |

### 待办

- 触摸多点能力: 真机确认后再决定是否承诺 `max_points > 1` 及相关 API.
- IMU `CTRL1` 的 `BE` 位与手册描述不一致: 需要绝对精度时用静态姿态基准复测字节序.

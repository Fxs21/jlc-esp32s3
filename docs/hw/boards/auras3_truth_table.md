# AuraS3 硬件真值表

提取方式:初版依据 `tmp/AuraS3` 中 Waveshare 官方 ESP-IDF BSP,Arduino `pin_config.h`,示例程序和器件资料整理;部分项目已由用户对照原理图或真机日志确认.本文件用于 BSP 实现和审核硬件事实,示例代码只能作为交叉参考,不能覆盖原理图结论.

## 0. 审核状态

| 项目 | 状态 | 备注 |
|---|---|---|
| 原理图逐页核对 | 部分确认 | `tmp/AuraS3/ESP32-S3-Touch-AMOLED-1.75/Schematic/ESP32-S3-Touch-AMOLED-1.75-schematic.pdf` |
| 官方 ESP-IDF BSP 对照 | 已整理 | `examples/ESP-IDF-v5.5/03_esp-brookesia/components/esp32_s3_touch_amoled_1_75` |
| Arduino pin 表对照 | 已整理 | `examples/Arduino-v3.3.5/libraries/Mylibrary/pin_config.h` |
| BSP 当前接入范围 | 已接入 | `display`,`touch`,`backlight`,`imu`,`audio`,`gnss`,`sdcard`,`pmu`;`camera` 无硬件 |
| 真机验证 | 部分通过 | `board`,`imu`,`sdcard`,`audio tone`,`ui`,`pmu` 已确认;GNSS 硬件未连接,暂不测 |

## 1. 板卡身份

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 板卡 | Waveshare ESP32-S3-Touch-AMOLED-1.75 | 官方 README / wiki 链接 |
| MCU | ESP32-S3 | 官方资料 |
| Display | CO5300 AMOLED | 官方 ESP-IDF BSP include `esp_lcd_co5300.h` |
| Display 分辨率 | `466 x 466` | 官方 BSP `BSP_LCD_H_RES` / `BSP_LCD_V_RES` |
| Display 接口 | QSPI | 官方 BSP `CO5300_PANEL_BUS_QSPI_CONFIG` |
| Touch | CST9217 | 官方 BSP include `esp_lcd_touch_cst9217.h` |
| IMU | QMI8658 | 真机 `i2c_scan` 和 BSP test 已确认 |
| Audio playback codec | ES8311 | 真机 `i2c_scan` 和 `audio tone` 已确认 |
| Audio record codec | ES7210 | 真机 `i2c_scan` 已确认;open path 已确认 |
| PMU | AXP2101 | 真机 `i2c_scan` 和 `pmu` test 已确认;当前 BSP 提供只读 public API |
| RTC | PCF85063 | 真机 `i2c_scan` 已确认;当前无 BSP public API |
| IO expander | TCA9554PWR | 真机 `i2c_scan` 地址 `0x20`;P7 用于 GPS reset |
| SD card | 1-bit SDMMC | CLK=GPIO2, CMD=GPIO1, D0=GPIO3;真机确认 |
| GNSS | LC76GABMD | 用户确认存在,接口为 UART;当前硬件未连接 |
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
| GNSS baudrate | `38400` | 当前 BSP 实现值;硬件未连接,待真机验证 |

### 总线冲突/待裁决项

- `01_AXP2101/sdkconfig.defaults` 写 `CONFIG_PMU_I2C_SCL=7` / `CONFIG_PMU_I2C_SDA=8`,与已确认主 pin 表 `SCL=14` / `SDA=15` 冲突;视为示例残留,实现以主 I2C 为准.
- Arduino `pin_config.h` 同时出现 `I2S_MCK_IO=16` 和实际 `MCLKPIN=42`;当前实现以官方 ESP-IDF BSP 和真机确认的 `GPIO42` 为准.
|- SD wiring 官方资料偏向 SDMMC 1-bit,当前 BSP 已使用 SDMMC 1-bit 并真机确认.

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
| 色彩格式 | RGB565,16bpp | native stream 为 high-byte-first RGB565 |
| 分辨率 | `466 x 466` | 官方 BSP / Arduino pin 表 |
| gap/offset | x gap `0x06`,y gap `0` | 当前 BSP 设置 |
| LVGL dirty alignment | `2 x 2` | 当前 BSP rounder 保证 x/y 偶数起点和奇数终点 |

### Display 初始化要点

- 当前 BSP 使用官方 `esp_lcd_co5300` QSPI panel driver,内部 panel owner 文件为 `auras3_panel.c`.
- CO5300 init table 已按厂家 QSPI/RGB565 序列收敛: `FE 00`,`C4 80`,`3A 55`,`35 00`,`53 20`,`51 00`,`63 FF`,`2A 00 06 01 D7`,`2B 00 00 01 D1`,`11` delay `60ms`,`29`.
- 厂家序列使用 `51 FF` 直接满亮;当前 BSP 保留 `51 00`,避免 init 阶段亮脏首帧,由 UI/backlight API 后续设置亮度.
- `bsp_display` public API 只提供 native async transfer + wait,不提供 `fill` 或 public host-order writer.
- AuraS3 native display contract 是 high-byte-first RGB565 byte stream;LVGL flush 必须执行 `lv_draw_sw_rgb565_swap()`.
- CO5300 QSPI 局部刷新区域需要 2 像素对齐: invalid area 的 `x1/y1` 向下取偶数,`x2/y2` 向上扩到奇数;未对齐时 LVGL demo 动态区域会出现残留.
- 该残留不是 AMOLED 物理残影,也不是 RGB565 endian 问题;它属于 CO5300/QSPI partial refresh 窗口约束.

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
| Mount point | `/sdcard` | FAT mount | shell test 真机确认 |

### SD 注意事项

- SDMMC 1-bit 模式真机验证通过: shell test mount 正常,与官方 BSP 行为一致.
- 此前使用过 SDSPI(SPI3_HOST + GPIO41 CS),真机 mount 31GB SDHC 卡成功.切换到 SDMMC 后,GPIO41 不再用于 SD card.
- SDIO 探测阶段 cmd=52 / cmd=5 command not supported 日志属于正常输出,只要 `mounted: 1` 即为成功.

## 7. IMU, QMI8658

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 芯片 | QMI8658 | 官方 demo / datasheet / 真机 test |
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
| PA enable | `GPIO46` | speaker amplifier enable | 官方 BSP `BSP_POWER_AMP_IO`;direct GPIO |

### Audio codec 角色

| 芯片 | 地址 | 角色 | 来源/备注 |
|---|---:|---|---|
| ES8311 | `0x18` | playback / DAC | 真机 `i2c_scan` + `audio tone` 确认 |
| ES7210 | 7-bit `0x40` | record / ADC | 真机 `i2c_scan` + open path 确认 |

### Audio 注意事项

- 当前 BSP 支持 ES8311 speaker playback 和 ES7210 MIC1/MIC2 16-bit stereo record.
- `audio tone 1000 1000` 已真机播放正常.
- ES8311 private driver 对 `GPIO_REG44` 首次写失败做 retry;该寄存器是 ES8311 内部 `0x44`,不是 ESP32 GPIO44.
- `audio rec-rms` 的 MIC1/MIC2 RMS 仍建议补测.
- MIC3 playback reference,TDM,AEC 当前暂停,不进入稳定 API.

## 9. PMU, AXP2101

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 芯片 | AXP2101 | 示例 / datasheet / 真机 scan |
| I2C 地址 | `0x34` | AuraS3 真机 `i2c_scan` 确认 |
| I2C SDA/SCL | 主 I2C `GPIO15` / `GPIO14` | 用户确认 AXP2101 挂主 I2C;PMU 示例 defaults `8/7` 视为残留 |
| IRQ | TCA9554 P5 `AXP_IRQ` | 用户确认 |
| 能力 | 电池/充电/VBUS/温度/系统电压/KEY2 事件 | 真机 test 和 AXP2101 driver 已确认 |

### PMU 当前结论

- 当前 BSP 已提供 `bsp_pmu` public API,只包含 `open` / `close` / `get_status` / `get_events`.
- 已确认 `KEY2` 短按 / 长按 event,长按硬关机,关机后按 `KEY2` 重新开机.
- 已确认 `SYS_OUT` idle 为 `0`,按下 `KEY2` 为 `1`;`AXP_IRQ` idle 为 `1`,pending IRQ 为 `0`.
- 已确认电池插入 / 拔出,充电开始,VBUS/电池/system voltage,PMU 温度和 `battery_percent` 读取.
- public API 不暴露 raw AXP2101 register,不开放 software power-off,rail control 或充电参数配置.详细结论见 `docs/hw/auras3-pmu-key.md`.

## 10. RTC, PCF85063

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 芯片 | PCF85063 | Arduino RTC demo / datasheet / 真机 scan |
| 总线 | 主 I2C | Arduino demo 使用 `Wire.begin(IIC_SDA,IIC_SCL)` |
| I2C 地址 | `0x51` | AuraS3 真机 `i2c_scan` 确认 |
| interrupt/CLKOUT | TCA9554 P3 `RTC_INT` | 用户确认 |

### RTC 实现建议

- 当前项目没有 RTC public BSP API,第一阶段不加入.
- 如果应用需要时间保持,后续单独设计 `bsp_rtc` 或交给应用层 driver.

## 11. GNSS

| 项目 | 真值 | 来源/备注 |
|---|---|---|
| 模块 | LC76GABMD | 用户确认;`device/MAX-M10S-00B-01.pdf` 不作为本板 GNSS 真值 |
| 接口 | UART | 用户确认;Arduino I2C 示例不作为本板接口真值 |
| UART TX/RX | ESP32 RX `GPIO18` <- GNSS TX;ESP32 TX `GPIO17` -> GNSS RX | 用户确认 |
| baudrate | `38400` | 当前 BSP 实现;硬件未连接,待真机验证 |
| reset | TCA9554 P7 `GPS_RST` | 当前 BSP 通过 IO expander 释放 reset |

### GNSS 注意事项

- 当前硬件未连接 GPS,`gnss` test / shell `gnss read` 的 `no data` 不作为失败结论.
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
- AuraS3 不声明 `BSP_BOARD_CAP_CAMERA`.
- 不要为了 shell 增加 AuraS3 camera capture 命令.

## 13. 当前 BSP 映射

| BSP 模块 | 当前状态 | 依据 |
|---|---|---|
| `bsp_board` | 已实现 | board test 已确认 capabilities 包含 display,touch,backlight,sdcard,gnss,imu,audio,pmu;camera no |
| `bsp_display` | 已实现 | CO5300 QSPI native async transfer,UI 真机确认 |
| `bsp_ui` | 已实现 | LVGL demo widgets 真机确认 |
| `bsp_backlight` | 已实现 | CO5300 `0x51` brightness percent mapping |
| `bsp_touch` | 已实现 | CST9217 touch 方向已确认 |
| `bsp_sdcard` | 已实现 | SDMMC 1-bit,真机确认 |
| `bsp_imu` | 已实现 | QMI8658 test 真机确认 |
| `bsp_audio` | 已实现 | ES8311 tone 真机确认;ES7210 open 正常 |
| `bsp_gnss` | 已实现但未硬件验证 | 模块未连接,暂不测 |
| `bsp_pmu` | 已实现 | AXP2101 只读 status/events,KEY2 和电池事件真机确认 |
| `bsp_camera` | unsupported | 用户确认无 camera |
| RTC | 暂缓 | 当前无 public BSP API |
| TCA9554PWR | 内部 helper 已接入 | 当前用于 GNSS reset 和 input default setup |

## 14. 建议审核清单

- 主 I2C 已确认: `GPIO15(SDA)` / `GPIO14(SCL)`.
- I2C scan 已确认: ES8311 `0x18`,TCA9554PWR `0x20`,AXP2101 `0x34`,ES7210 `0x40`,PCF85063 `0x51`,CST9217 `0x5A`,QMI8658 `0x6B`.
- TCA9554PWR P0..P7 当前记录为: P0/P1/P2 NC,P3 RTC_INT,P4 SYS_OUT,P5 AXP_IRQ,P6 QMI_INT,P7 GPS_RST.
- I2S MCLK 已确认: `GPIO42`,忽略 `I2S_MCK_IO=16` 残留定义.
- SD 当前 BSP 采用 SDMMC 1-bit(CLK=GPIO2,CMD=GPIO1,D0=GPIO3);真机验证已通过.
- GNSS 已确认: LC76GABMD + UART,ESP32 RX `GPIO18` / TX `GPIO17`;硬件未连接,baud `38400` 和 reset 极性仍待真机确认.
- Camera 已确认不存在.
- CO5300 brightness 通过 panel command `0x51` 控制,无独立 BL GPIO.
- PMU 已确认只读状态和 mapped events 可用;software power-off,rail control 和充电参数配置仍需后续专项验证.

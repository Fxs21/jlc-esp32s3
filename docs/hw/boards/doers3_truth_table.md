# ESP32-S3 DoerS3 板级硬件真值表

提取方式:主要依据原理图 P1/P2/P3 和当前 BSP 真机验证记录.本文档记录硬件事实和当前软件约定,不作为 public API 设计本身.

## 1. 总线与地址

| 项目 | 原理图真值 / 当前约定 |
|---|---|
| 主 I2C | `IO1_I2C_SDA` / `IO2_I2C_SCL` |
| QMI8658 地址 | `0x6A` |
| PCA9557 地址 | `0x19` |
| FT6X36 地址 | `0x38` |
| ES7210 地址 | `0x41` 7-bit |
| ES8311 地址 | `0x18` 7-bit |

当前 BSP 使用 common I2C bus owner,DoerS3 board wrapper 负责传入 `IO1/IO2` pin,port 和 speed.

## 2. IO 扩展, PCA9557

| PCA9557 引脚 | 连接信号 | 当前 BSP 用途 |
|---|---|---|
| IO0 | `LCD_CS` | ST7789 panel CS |
| IO1 | `PA_EN` | NS4150B speaker PA enable |
| IO2 | `DVP_PWDN` | GC0308 camera power-down,`0` 为工作态 |

PCA9557 是 DoerS3 board-private helper,不进入 public BSP API.

## 3. LCD / Touch / SD / Camera / Audio / GPS 关键信号

### LCD, ST7789 TFT

| 信号 | ESP32-S3 / 板级网名 | 备注 |
|---|---|---|
| DC | `IO39_LCD_DC` | ST7789 D/C |
| MOSI | `IO40_LCD_MOSI` | SPI data |
| SCK | `IO41_LCD_SCK` | SPI clock |
| CS | `LCD_CS` | 来自 PCA9557 IO0 |
| Backlight | `IO42_LCD_BL` | LEDC backlight |

当前 BSP 约定:

- Display native contract 为 little-endian RGB565.
- ST7789 `.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE`.
- DoerS3 LVGL flush 直接传 LVGL RGB565 buffer,不做 byte swap.
- Display/UI 真机已确认正常.

### Touch, FT6X36/FT6336

| 信号 | 连接 |
|---|---|
| I2C SDA | `IO1_I2C_SDA` |
| I2C SCL | `IO2_I2C_SCL` |
| RESET | `RESET` |
| I2C 地址 | `0x38` |

当前 BSP 使用 private `ft6336` driver,public API 返回 BSP 自有 touch point 结构.

### SD card, 1-bit SDMMC

| 信号 | 连接 |
|---|---|
| CMD | `IO48_SD_CMD` |
| CLK | `IO47_SD_CLK` |
| DAT0 | `IO21_SD_DAT0` |

当前 BSP 支持 FAT mount,DoerS3 真机已确认 SD 卡可挂载.

### DVP 摄像头, GC0308

| 信号 | 连接 |
|---|---|
| SCCB SDA | `IO1_I2C_SDA` |
| SCCB SCL | `IO2_I2C_SCL` |
| PWDN | `DVP_PWDN`,来自 PCA9557 IO2 |
| VSYNC | `IO3_DVP_VSYNC` |
| HREF | `IO46_DVP_HREF` |
| PCLK | `IO7_DVP_PCLK` |
| XCLK | `IO5_DVP_XCLK` |
| D0 | `IO16_DVP_D0` |
| D1 | `IO18_DVP_D1` |
| D2 | `IO8_DVP_D2` |
| D3 | `IO17_DVP_D3` |
| D4 | `IO15_DVP_D4` |
| D5 | `IO6_DVP_D5` |
| D6 | `IO4_DVP_D6` |
| D7 | `IO9_DVP_D7` |

当前 BSP 配置:

- Sensor: GC0308.
- Frame: QVGA `320x240`,RGB565.
- XCLK: `20MHz`.
- SCCB 使用 DoerS3 公共 I2C bus.
- `DVP_PWDN=0` 为工作态.
- Public camera API 只承诺单帧采集: `open -> capture -> release_frame -> close`.

### Audio, ES8311 + ES7210

#### Audio 控制与 I2S

| 信号 | ESP32-S3 / 板级网名 | 连接到 | 用途 |
|---|---|---|---|
| I2C SDA | `IO1_I2C_SDA` | ES8311 / ES7210 | codec 控制总线 |
| I2C SCL | `IO2_I2C_SCL` | ES8311 / ES7210 | codec 控制总线 |
| MCLK | `IO38_I2S_MCK` | ES8311 `MCLK` + ES7210 `MCLK` | 两个 codec 共用主时钟 |
| BCLK | `IO14_I2S_BCK` | ES8311 `SCLK` + ES7210 `SCLK` | 两个 codec 共用 bit clock |
| LRCK/WS | `IO13_I2S_WS` | ES8311 `LRCK` + ES7210 `LRCK` | 两个 codec 共用 word select |
| I2S DO | `IO45_I2S_DO` | ES8311 `DSDIN` | ESP32-S3 播放数据输出到 ES8311 DAC |
| I2S DI | `IO12_I2S_DI` | ES7210 `SDOUT1/TDMOUT`,串 `R36=51R` | ES7210 录音/TDM 数据输出到 ESP32-S3 |
| PA EN | `PA_EN` | NS4150B `CTRL`,来自 PCA9557 IO1 | 功放使能,默认下拉关闭 |

#### Codec 地址与角色

| 芯片 | 7-bit I2C 地址 | 原理图角色 | BSP 默认角色 |
|---|---:|---|---|
| ES8311 | `0x18` | DAC 输出到功放;`ASDOUT` 未接;`MIC1P/MIC1N/MICBIAS` 未用 | playback codec,仅 DAC |
| ES7210 | `0x41` | 多路 ADC,`SDOUT1/TDMOUT` 接 ESP32-S3 `DI` | record codec,默认 MIC1/MIC2 stereo |

#### 模拟音频路径

| 路径 | 原理图连接 | BSP 含义 |
|---|---|---|
| 播放 | ESP32-S3 `IO45_I2S_DO` -> ES8311 `DSDIN` -> ES8311 `OUTP/OUTN` -> NS4150B -> 喇叭接口 | 默认播放路径 |
| 板载双麦 | MIC1 -> ES7210 `MIC1P/MIC1N`;MIC2 -> ES7210 `MIC2P/MIC2N`;`MICBIAS12` 给 MIC1/MIC2 偏置 | 默认录音路径,16-bit stereo |
| 播放回采 | ES8311 `OUTP/OUTN` -> `R34/R35=0R` -> ES7210 `MIC3P/MIC3N` | 硬件上可作为 loopback/reference;当前 AEC/TDM 调试暂停,不作为 BSP 公开录音能力 |
| 未使用 | ES8311 `ASDOUT` 未接;ES8311 `MIC1P/MIC1N/MICBIAS` 未接;ES7210 `SDOUT2/TDMIN` 侧 `R37=0R NC` | 不应设计为 ES8311 ADC 录音或 codec 级联 TDM |

#### Audio 注意事项

- 默认录音只承诺 ES7210 `MIC1/MIC2` 双麦 stereo;不要把 ES8311 当作录音 ADC 使用.
- ES7210 `MIC3` 是 ES8311 模拟输出回采,不是第三个板载麦克风.
- 只录 `MIC1/MIC2` 时使用 standard I2S stereo 即可,不需要 TDM.
- 若后续要同时录 `MIC1/MIC2` 和播放回采,应作为独立专项扩展 ES7210 `TDMOUT` + ESP32-S3 TDM RX,而不是启用 ES8311 ADC.
- 播放和录音共用 `MCLK/BCLK/LRCK`,同时启用时 sample rate / bit width / frame 配置必须一致.
- DoerS3 shell `audio tone` 已真机确认播放正常.

### GNSS, P1 外部接口 J2

| 信号 | 连接 |
|---|---|
| ESP32 RX | `IO10 <- GNSS_TX` |
| ESP32 TX | `IO11 -> GNSS_RX` |
| UART | `UART1` |
| Baudrate | `38400` |
| 当前验证模块 | MAX-M10S |

DoerS3 真机已确认可收到有效 NMEA,RMC/GGA parser 正常.

## 4. 当前 BSP 能力和验证状态

| BSP 模块 | DoerS3 状态 |
|---|---|
| `bsp_board` | 已实现,真机确认 |
| `bsp_display` | ST7789 已实现,little-endian native contract 真机确认 |
| `bsp_ui` | 已实现,真机确认 |
| `bsp_touch` | FT6336 已实现,真机确认 |
| `bsp_backlight` | LEDC backlight 已实现 |
| `bsp_sdcard` | 1-bit SDMMC mount 已实现,真机确认 |
| `bsp_imu` | QMI8658 已实现,真机确认 |
| `bsp_audio` | ES8311 playback + ES7210 MIC1/MIC2 record 已实现,真机确认 |
| `bsp_camera` | GC0308 QVGA RGB565 单帧采集已实现,真机确认 |
| `bsp_gnss` | MAX-M10S UART NMEA 已实现,真机确认 |

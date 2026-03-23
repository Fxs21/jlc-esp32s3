# ESP32-S3 板级硬件真值表（从原理图提取）

提取方式：仅依据原理图 P1/P2/P3，不依赖软件默认配置。

## 1) 总线与地址

| 项目 | 原理图真值 |
|---|---|
| 主 I2C | `IO1_I2C_SDA` / `IO2_I2C_SCL` |
| QMI8658 地址 | `0x6A`（P1） |
| PCA9557 地址 | `0x19`（P1） |
| FT6X36 地址 | `0x38`（P2） |
| ES7210 地址 | `0x41`（P3，7-bit 标注） |
| ES8311 地址 | `0x18`（P3，7-bit 标注） |

## 2) IO 扩展（PCA9557）功能分配

| PCA9557 引脚 | 连接信号 |
|---|---|
| IO0 | `LCD_CS` |
| IO1 | `PA_EN` |
| IO2 | `DVP_PWDN` |

## 3) LCD / Touch / SD / Camera / Audio / GPS 关键信号

### LCD（TFT 接口，P2）

- `IO39_LCD_DC`
- `IO40_LCD_MOSI`
- `IO41_LCD_SCK`
- `LCD_CS`（来自 PCA9557 IO0）
- `IO42_LCD_BL`（背光控制网名）

### Touch（电容触摸，P2）

- `IO1_I2C_SDA`
- `IO2_I2C_SCL`
- `RESET`
- I2C 地址：`0x38`

### SD 卡（P1，1-bit 模式）

- `IO48_SD_CMD`
- `IO47_SD_CLK`
- `IO21_SD_DAT0`

### DVP 摄像头（J6，P2）

- SCCB: `IO1_I2C_SDA` / `IO2_I2C_SCL`
- `DVP_PWDN`（来自 PCA9557 IO2）
- `IO3_DVP_VSYNC`
- `IO46_DVP_HREF`
- `IO7_DVP_PCLK`
- `IO5_DVP_XCLK`
- `IO4_DVP_D6`
- `IO6_DVP_D5`
- `IO15_DVP_D4`
- `IO17_DVP_D3`
- `IO8_DVP_D2`
- `IO18_DVP_D1`
- `IO16_DVP_D0`
- `IO9_DVP_D7`

### Audio（P3）

- I2C: `IO1_I2C_SDA` / `IO2_I2C_SCL`
- I2S: `IO38_I2S_MCK` / `IO14_I2S_BCK` / `IO13_I2S_WS` / `IO45_I2S_DO` / `IO12_I2S_DI`
- 功放使能：`PA_EN`（来自 PCA9557 IO1）

### GPS（P1 外部接口 J2）

- `IO10`
- `IO11`
- 常用映射（ATGM336H）：`IO10 <- GPS_TX`，`IO11 -> GPS_RX`
- 软件约定：GPS 固定使用 `UART1`

# ESP32-S3 DoerS3 板级硬件真值表

提取方式:主要依据同目录原理图 (`schematic/schematic.pdf`,分页图 `schematic/page-01..03.png`,对应 P1/P2/P3) 和当前 BSP 真机验证记录.本文档记录硬件事实和当前软件约定,不作为 public API 设计本身.

## 1. 总线与地址

| 项目 | 原理图真值 / 当前约定 |
|---|---|
| 主 I2C | `IO1_I2C_SDA` / `IO2_I2C_SCL` |
| QMI8658 地址 | `0x6A` |
| PCA9557 地址 | `0x19` |
| FT6X36 地址 | `0x38` |
| ES7210 地址 | `0x41` 7-bit |
| ES8311 地址 | `0x18` 7-bit |

- 自检 `test/i2c` (2026-10-08, sha `32e06a9`): 4/4 PASS; 扫描到上表 5 个设备, 总线上无其他在线地址.

当前 BSP 使用 common I2C bus owner,DoerS3 board wrapper 负责传入 `IO1/IO2` pin,port 和 speed.

## 2. IO 扩展, PCA9557

| PCA9557 引脚 | 连接信号 | 当前 BSP 用途 |
|---|---|---|
| IO0 | `LCD_CS` | ST7789 panel CS |
| IO1 | `PA_EN` | NS4150B speaker PA enable |
| IO2 | `DVP_PWDN` | GC0308 camera power-down,`0` 为工作态 |

PCA9557 是 DoerS3 board-private helper,不进入 public BSP API.

## 3. LCD / Touch / IMU / SD / Camera / Audio / GPS 关键信号

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

新式自检复测 (2026-10-09, sha `f416fd8`, `test/bsp.sh display doers3`): 7/7 PASS, 3 个人工项 `yes`.

- 覆盖: desc/open-close, 二次 open 拒绝, 参数校验, done 回调计数, 分块写, 全屏五色顺序, 居中图案对称, 左边缘标记.
- 复测期间发现 `fill_rect` 在异步传输未完成时释放并复用 strip, frame 图案底部残留白条;修复为等 done 回调到齐再释放, 同 sha 复测干净通过.

### Touch, FT6X36/FT6336

| 信号 | 连接 |
|---|---|
| I2C SDA | `IO1_I2C_SDA` |
| I2C SCL | `IO2_I2C_SCL` |
| RESET | `RESET` |
| I2C 地址 | `0x38` |

当前 BSP 使用 private `ft6336` driver,public API 返回 BSP 自有 touch point 结构.

### IMU, QMI8658

| 信号 | 连接/值 | 备注 |
|---|---|---|
| I2C SDA / SCL | `IO1_I2C_SDA` / `IO2_I2C_SCL` | 主 I2C |
| I2C 地址 | `0x6A` | 真机 `i2c_scan` 确认 |
| 中断脚 | 未连接 | 当前 BSP polling,不使用中断 |

新式自检复测 (2026-10-09, sha `e6ed4b6`, `test/bsp.sh imu doers3`): 6/6 PASS.

- `open()` 契约: 只完成配置和独占,单位 `m/s^2` / `rad/s`;首样本由调用方用 `bsp_imu_is_data_ready()` 把关.
- 实测 accel 模长 9.91~9.96 m/s^2,静态 gyro 噪声约 0.14 rad/s,温度约 31.1 ℃.
- `CTRL1` 的 `BE` 位实证: 写 `0x60` (`BE=1`) 时芯片仍按小端输出;现驱动显式写 `0x40` (`ADDR_AI=1`, `BE=0`) 与小端解析对齐,手册 Table 22 与实测矛盾处按实测记录.

### SD card, 1-bit SDMMC

| 信号 | 连接 |
|---|---|
| CMD | `IO48_SD_CMD` |
| CLK | `IO47_SD_CLK` |
| DAT0 | `IO21_SD_DAT0` |

当前 BSP 支持 FAT mount,DoerS3 真机已确认 SD 卡可挂载.

新式自检复测 (2026-10-09, sha `5fecc25`, `test/bsp.sh sdcard doers3`): 8/8 PASS.

- 卡信息: `SDABC`, SDHC, 30003.5 MB, 1-bit @ 20 MHz;覆盖 mount/unmount, 信息查询, 挂载点校验和 4096 字节写读回.

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

test_app 验证:

- viewfinder 连续 200 帧: `capture -> byte-swap -> bsp_display_write`,真机通过.
- byte-swap 原因: camera 输出 big-endian RGB565,display native contract 为 little-endian RGB565.
- 实测约 10 FPS (QVGA RGB565 ~153KB/frame + SPI DMA `@80MHz`).

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
| 播放回采 | ES8311 `OUTP/OUTN` -> `R34/R35=0R` -> ES7210 `MIC3P/MIC3N` | 回采通道 (TDM slot 1);BSP 已实现同一设计,DoerS3 未复测 |
| 未使用 | ES8311 `ASDOUT` 未接;ES8311 `MIC1P/MIC1N/MICBIAS` 未接;ES7210 `SDOUT2/TDMIN` 侧 `R37=0R NC` | 不应设计为 ES8311 ADC 录音或 codec 级联 TDM |

#### Audio 注意事项

- 录音走 ES7210;不要把 ES8311 当作录音 ADC 使用 (`ASDOUT` 未接).
- ES7210 `MIC3` 是 ES8311 模拟输出回采,不是第三个板载麦克风.
- 只录 `MIC1/MIC2` 时使用 standard I2S stereo 即可,不需要 TDM.
- 同时录 `MIC1/MIC2` 和回采由 TDM 会话实现,通道顺序 `[MIC1, 回采, MIC2]`;语义见 `docs/bsp/audio.md`,DoerS3 未复测.
- 播放和录音共用 `MCLK/BCLK/LRCK`,同时启用时 sample rate / bit width / frame 配置必须一致.
- 播放路径真机确认过 (旧口径);新式自检和回采待复测,`supports_full_duplex` 暂为 `false`.

### GNSS, P1 外部接口 J2

| 信号 | 连接 |
|---|---|
| ESP32 RX | `IO10 <- GNSS_TX` |
| ESP32 TX | `IO11 -> GNSS_RX` |
| UART | `UART1` |
| Baudrate | `38400` |
| 当前验证模块 | MAX-M10S |

DoerS3 真机已确认可收到有效 NMEA,RMC/GGA parser 正常.模块资料: `docs/hw/specs/chips/max-m10s_datasheet.pdf`.

## 4. 当前 BSP 能力和验证状态

| BSP 模块 | DoerS3 状态 |
|---|---|
| `bsp_board` | 已实现,真机确认 |
| `bsp_display` | ST7789 已实现,little-endian native contract 真机确认;新式自检 7/7 PASS (2026-10-09, sha `f416fd8`) |
| `bsp_ui` | 已实现,真机确认 |
| `bsp_touch` | FT6336 已实现,真机确认 |
| `bsp_backlight` | LEDC backlight 已实现 |
| `bsp_sdcard` | 1-bit SDMMC mount 已实现,真机确认 |
| `bsp_imu` | QMI8658 已实现;新式自检 6/6 PASS (2026-10-09, sha `e6ed4b6`) |
| `bsp_audio` | 会话模型 (record mask,TDM 回采,full-duplex) 已对齐实现;播放真机确认,新式自检未复测 |
| `bsp_camera` | GC0308 QVGA RGB565 单帧采集已实现,真机确认 |
| `bsp_gnss` | MAX-M10S UART NMEA 已实现,真机确认 |

## 5. 例程对照结论

对照 `docs/code/doers3/` 例程源码,逐项核对初始化序列,地址和取值;一致项已由 driver 覆盖,差异项按下表记录"例程行为 / 本仓库取舍 / 理由".核对基线: 2026-10-09, sha `e6ed4b6`.已核对完毕的例程从 `docs/code/` 删除.

### 02-attitude -> `bsp_imu`

一致项: 地址 `0x6A`,WHO_AM_I `0x05`,`CTRL1` 地址自增,`CTRL7` bit0/bit1 使能 accel/gyro,`STATUS0 & 0x03` 判数据就绪,`AX_L` 起 12 字节 burst 小端解析.

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| `RESET` (`0x60`) 写 `0xB0` 后等 10 ms | 不复位,直接写配置 | 复位不是硬件约束;上电默认配置下直接配置,双板真机复测稳定 |
| `CTRL1` 写 `0x40` (`ADDR_AI=1`, `BE=0`) | 同样写 `0x40` (此前沿用 `BE=1`,已改为显式 `BE=0`) | 真机实证 `BE=1` 时芯片仍按小端输出,手册 Table 22 与实测矛盾处按实测;显式 `BE=0` 让配置和解析路径一致 |
| accel `4G@250Hz` (`CTRL2=0x95`,含 self-test 位) | accel `8G@1000Hz` (`CTRL2=0x23`),不开 self-test | 覆盖更大动态范围;self-test 是产测动作,不放进 `open()` |
| gyro `512dps@250Hz` (`CTRL3=0xD5`,含 self-test 位) | gyro 设定 `512dps@1000Hz` (`CTRL3=0x43`),不开 self-test | 本仓库自有取值;`CTRL3=0x43` 的 `gFS` 换算见 `docs/bsp/status.md` 挂起项 |
| `CTRL7` 使能后不等首样本,主循环未就绪时沿用旧值 | `open()` 只配置,调用方用 `bsp_imu_is_data_ready()` 等首样本 | 明确契约,避免把未产出的零值样本当有效读数 |
| I2C `100 kHz` | `400 kHz` | 与全板 I2C 速率统一 |

### 03-micro_sd -> `bsp_sdcard`

一致项: 引脚 CLK `IO47` / CMD `IO48` / D0 `IO21`,1-bit SDMMC,内部上拉,`max_files=5`,20 MHz,挂载点 `/sdcard`.

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| `format_if_mount_failed = true` | `false` | 格式化是破坏性操作,BSP 不自动执行 |
| `allocation_unit_size = 16 KB` | 32 KB | 批量写场景减少 FAT 簇链开销;该值只影响性能,不影响挂载兼容性 |
| `CONFIG_FATFS_VFS_FSTAT_BLKSIZE=4096` | IDF 默认 | 当前没有对该值敏感的使用路径;需要时再对齐 |

### 06-lcd -> `bsp_display`

一致项: MOSI `IO40` / SCLK `IO41` / DC `IO39` / RST 未接, 背光 `IO42`;PCA9557 `LCD_CS` (IO0);SPI mode 2, cmd/param 8 bit, 16 bpp, `320x240`, `pclk 80 MHz`, `trans_queue_depth 10`;`invert_color(true)` + `swap_xy(true)` + `mirror(true, false)`;LEDC 5 kHz, `output_invert=true`.

| 例程行为 | 本仓库取舍 | 理由 |
|---|---|---|
| `SPI3_HOST` | `SPI2_HOST` | 板级自由选择;与 AuraS3 board port 统一 |
| 未设置 `data_endian`, 面板 `RAMCTL` 保持默认 big-endian | `data_endian = LCD_RGB_DATA_ENDIAN_LITTLE` (面板 `RAMCTL` 置 little-endian 位) | 调用方直接传 LVGL / host-order RGB565, flush 路径不逐像素换序 |
| LEDC `LEDC_TIMER_1`, 10-bit duty (100% = 1023) | `LEDC_TIMER_0`, 13-bit duty (100% = 8191) | timer0 与 camera XCLK 的 `LEDC_TIMER_1` 分开, backlight 与 camera 在同一镜像共存;13-bit 是 5 kHz 下可取的最高分辨率 |
| 背光 percent -> 1023 取整 | percent -> 8191 取整, 百分比语义不变 | 档位更细, API 语义一致 |

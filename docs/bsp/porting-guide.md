# Board Port 接入指南

新增一块板子时,按本文档创建 board port 文件并接入构建系统.

## 1. 文件总览

每块板子放在 `components/bsp/src/boards/<board>/` 下,包含以下文件:

```
src/boards/<board>/
  board.c              # bsp_board_get_info() + bsp_i2c_get_config()
  board.cmake          # 本板的 source manifest

  display.c            # bsp_display_* + bsp_display_port_lvgl_open/close() (AuraS3 含 backlight)
  touch.c              # bsp_touch_* public API
  backlight.c          # bsp_backlight_* public API (或合入 display.c)
  sdcard.c             # bsp_sdcard_get_pins()
  imu.c                # bsp_imu_* public API
  audio.c              # bsp_audio_* public API (委托 common/audio.c)
  gnss.c               # bsp_gnss_* public API

  internal/
    pins.h             # 所有板级 pin 宏 (硬件的单一事实来源)

  # 以下可选,按需要添加:
  pmu.c                          # bsp_pmu_* public API (仅 AuraS3)
  camera.c                       # bsp_camera_* public API (仅 DoerS3 + CONFIG_BSP_ENABLE_CAMERA)
  internal/<board>_ioexp.c       # IOEXP acquire/release/set_pin
  internal/<board>_ioexp.h       # IOEXP semantic inline wrappers
```

当前已有的 board port:

- `doers3`: ST7789 SPI display, FT6336 touch, PCA9557 IOEXP, LEDC backlight, SDMMC 1-bit, QMI8658 IMU, ES8311+ES7210 audio, GC0308 camera, MAX-M10S GNSS
- `auras3`: CO5300 QSPI display, CST9217 touch, TCA9554 IOEXP, CO5300 cmd brightness, SDMMC 1-bit, QMI8658 IMU, ES8311+ES7210 audio, LC76GABMD GNSS, AXP2101 PMU

## 2. board.cmake

每块板单独的 source manifest. 编译时只链接当前选中的板.

```cmake
# src/boards/<board>/board.cmake
set(BSP_BOARD_SRCS
    "src/boards/<board>/board.c"
    "src/boards/<board>/display.c"
    "src/boards/<board>/touch.c"
    "src/boards/<board>/backlight.c"
    "src/boards/<board>/imu.c"
    "src/boards/<board>/audio.c"
    "src/boards/<board>/gnss.c"
    "src/boards/<board>/sdcard.c"
)

set(BSP_BOARD_UNSUPPORTED
    "pmu"    # 本板没有 PMU
)
```

规则:

- `BSP_BOARD_SRCS`: board port 源文件,不要手写 unsupported stub
- `BSP_BOARD_UNSUPPORTED`: 本板没有的 capability (如 `pmu`,`camera`);对应共享 stub 由 `components/bsp/CMakeLists.txt` 兜底追加,未注册 stub 的 capability 会在 configure 阶段报错
- `BSP_BOARD_DRIVERS`: 私有 IC driver (如 `src/drivers/touch/ft6336.c`)
- `BSP_BOARD_PRIV_INCLUDE_DIRS`: 需要 include 的 private 目录 (如 `src/boards/<board>/internal`)
- `BSP_BOARD_PRIV_LINK_LIBS`: 需要额外链接的 managed component (如 `idf::espressif__esp_lcd_co5300`)
- camera 可选: `if(CONFIG_BSP_ENABLE_CAMERA)` 时把实现加入 `BSP_BOARD_SRCS`,`else()` 时把 `camera` 加入 `BSP_BOARD_UNSUPPORTED`

参考 `components/bsp/src/boards/doers3/board.cmake` 和 `components/bsp/src/boards/auras3/board.cmake`.

在 `components/bsp/CMakeLists.txt` 中注册新板:

```cmake
if(CONFIG_BSP_BOARD_DOERS3)
    include("${CMAKE_CURRENT_LIST_DIR}/src/boards/doers3/board.cmake")
elseif(CONFIG_BSP_BOARD_AURAS3)
    include("${CMAKE_CURRENT_LIST_DIR}/src/boards/auras3/board.cmake")
elseif(CONFIG_BSP_BOARD_<BOARD>)
    include("${CMAKE_CURRENT_LIST_DIR}/src/boards/<board>/board.cmake")
endif()
```

同时需要在 Kconfig 中添加 `CONFIG_BSP_BOARD_<BOARD>` 选项.

## 3. internal/pins.h

板级 pin 分配的唯一定义点. 所有 board port 实现文件都通过 include 此头文件获取 pin 宏.

文件应包含: I2C, display, touch, backlight, SD card, audio, IMU, GNSS, camera, IOEXP 等所有硬件信号的 GPIO/port 宏定义. 注意:

- 每个宏都必须以 `<BOARD>_` 前缀命名,例如 `DOERS3_LCD_DC`, `AURAS3_LCD_CS`
- 不再使用的 pin 保留注释说明历史用途,不要直接删除. 例如 `// GPIO41 was the SDSPI CS pin; now free (SDMMC 1-bit needs only CLK/CMD/D0).`
- `auras3_pins.h` 和 `doers3_pins.h` 都在各自 `internal/` 下,可直接参考模板写法

## 4. board.c

实现两个功能:

### 4.1. bsp_board_get_info()

```c
#include "bsp_board.h"

static const bsp_board_info_t s_info = {
    .id = BSP_BOARD_ID_<BOARD>,
    .name = "<BoardName>",
};

const bsp_board_info_t *bsp_board_get_info(void)
{
    return &s_info;
}
```

### 4.2. bsp_i2c_get_config()

提供 board 专用的 I2C bus 配置. 结构和声明都在内部头 `src/common/bsp_i2c_internal.h`,不进入公开 API;`src/common/bsp_i2c.c` 在首次 `bsp_i2c_acquire()` 时读取它并初始化 bus.

```c
#include "bsp_i2c_internal.h"
#include "<board>_pins.h"

const bsp_i2c_bus_config_t *bsp_i2c_get_config(void)
{
    static const bsp_i2c_bus_config_t cfg = {
        .port = <BOARD>_I2C_PORT,
        .sda = <BOARD>_I2C_SDA,
        .scl = <BOARD>_I2C_SCL,
        .glitch_ignore_cnt = <BOARD>_I2C_GLITCH_IGNORE_CNT,
        .internal_pullup = <BOARD>_I2C_INTERNAL_PULLUP,
    };
    return &cfg;
}
```

参考: `doers3/board.c`, `auras3/board.c`.

## 5. display.c

### 5.1. struct bsp_display_s

每个 board 定义自己的 display handle 结构体:

```c
struct bsp_display_s {
    <panel_handle_type> panel;      // 私有 panel driver handle
    bsp_lvgl_port_t port;           // common LVGL port sub-struct
    // 可选: bool ioexp_acquired;
};
```

两个 board 的 display.c 均直接使用 `esp_lcd_panel_handle_t + esp_lcd_panel_io_handle_t`,不再经过中间 driver 封装.

### 5.2. Public API 实现

必须实现以下所有函数:

| 函数 | 说明 |
|------|------|
| `bsp_display_open()` | 初始化和配置 panel, 检查 `!s_display_open` |
| `bsp_display_close()` | 检查 `handle->port.lv_display == NULL`, 释放资源 |
| `bsp_display_get_info()` | 返回 `bsp_display_info_t`, handle 参数仅用于类型安全,可传入 NULL |
| `bsp_display_set_done_cb()` | 通过 panel driver 注册 transfer done callback |
| `bsp_display_write()` | 校验参数后写入 native pixel data |
| `bsp_display_port_lvgl_open()` | 内部 LVGL port 初始化, 设置 flush callback |
| `bsp_display_port_lvgl_close()` | 内部 LVGL port 反初始化 |

### 5.3. 通用模式

- open 失败路径用 `bsp_display_close()` 回滚, 不要在 err 标签里手写各步释放
- close 必须检查 `handle->port.lv_display == NULL` 防止 LVGL port 未关闭就释放 panel
- `validate_write_args()` 函数检查坐标范围 + data_size 是否符合 `width * height * bpp / 8`
- LVGL flush callback 调用 `bsp_display_write()`, 失败时仍要调用 `lv_display_flush_ready()`
- `bsp_display_port_lvgl_open/close` 在 `src/common/bsp_display_internal.h` 中声明, 由 board port 实现

### 5.4. 新增 board 需要决定的配置

- Panel driver: SPI (ST7789) 或 QSPI (CO5300) 或其他
- Native byte order: little-endian (DoerS3 ST7789) 或 high-byte-first (AuraS3 CO5300)
- LVGL flush 是否需要 rgb565_swap
- 是否需要 LVGL rounder (CO5300 QSPI 需要 2-pixel alignment)
- Transfer done callback 适配 (SPI 用 panel driver 回调, QSPI 用 esp_lcd_panel_io 回调)

参考: `doers3/display.c`, `auras3/display.c`.

## 6. touch.c

实现 `bsp_touch_get_desc()`, `bsp_touch_open()`, `bsp_touch_close()`, `bsp_touch_read()`.

通用模式:

- open 中 `bsp_i2c_acquire()` + 初始化 touch driver
- close 中释放 driver + `bsp_i2c_release()`
- read 从私有 driver 读取 raw point, 复制到 `bsp_touch_point_t`
- desc 中 `max_points` 设 1 (当前所有板都只支持单点)
- `x_max` / `y_max` 基于 display 分辨率 (注意 DoerS3 是 swap_xy)

参考: `doers3/touch.c` (FT6336), `auras3/touch.c` (CST9217).

## 7. backlight.c

实现 `bsp_backlight_get_desc()`, `bsp_backlight_open()`, `bsp_backlight_close()`, `bsp_backlight_set_percent()`, `bsp_backlight_get_percent()`.

两种模式:

- **PWM mode** (DoerS3): 使用 `backlight_ledc` driver, 需要 pins.h 中指定 GPIO + invert
- **Panel command mode** (AuraS3): 通过 CO5300 panel driver 的 brightness command, 在 `auras3/display.c` 内实现, 通过共享的 s_panel ref_count 管理 panel 生命周期

通用模式:

- desc 的 `present` = true
- 亮度 0..100 是 public 语义, board port 负责映射到具体硬件范围
- `set_percent` 对 `>100` 做 clamp
- AuraS3 的 backlight 和 display 在同一个 `auras3/display.c` 文件中,通过共享的 ref_count 管理 panel 生命周期

## 8. sdcard.c

只提供 pin 配置. SD card mount/unmount 全部在 `src/common/sdcard.c` 中实现.

```c
#include "sdcard.h"
#include "<board>_pins.h"

static const bsp_sdcard_pins_t s_pins = {
    .clk = <BOARD>_SDCARD_CLK,
    .cmd = <BOARD>_SDCARD_CMD,
    .d0  = <BOARD>_SDCARD_D0,
};

const bsp_sdcard_pins_t *bsp_sdcard_get_pins(void)
{
    return &s_pins;
}
```

约 14 行. 两块板完全相同的模式. 仅 pins.h 中的 GPIO 值不同.

当前两块板都使用 SDMMC 1-bit 模式. `src/common/sdcard.h` 定义了 `bsp_sdcard_pins_t` 结构.

## 9. imu.c

实现 `bsp_imu_get_desc()`, `bsp_imu_open()`, `bsp_imu_close()`, `bsp_imu_read()`, `bsp_imu_is_data_ready()`.

通用模式:

- open 中 `bsp_i2c_acquire()` + QMI8658 init + 设置单位
- 如果主地址失败可以 fallback 到次地址 (AuraS3 模式)
- close 中 `i2c_master_bus_rm_device()` + `bsp_i2c_release()`
- read 从 QMI8658 读取数据, 映射到 `bsp_imu_data_t` (带单位字段名)

参考: `doers3/imu.c` (单一地址), `auras3/imu.c` (双地址 fallback).

## 10. audio.c

Board port 实现 `bsp_audio_*` public API, 内部委托给 `src/common/audio.c` 的 common 层.

### 10.1. Board port 必须提供

1. `bsp_audio_pins_t` 结构: I2S port, MCLK/BCLK/WS/DOUT/DIN GPIO, ES8311/ES7210 I2C 地址
2. `bsp_audio_pa_fn` callback: PA 使能/关闭 (DoerS3 通过 IOEXP, AuraS3 通过 direct GPIO)
3. `bsp_audio_get_desc()` 和 `bsp_audio_default_config()`

### 10.2. 常见实现模式

```c
struct bsp_audio_s {
    bsp_audio_common_t *common;
    bool i2c_acquired;
    // DoerS3 额外: bool ioexp_acquired;
};
```

- open: `bsp_i2c_acquire()` + 配置 PA GPIO (AuraS3 direct GPIO) 或 acquire IOEXP (DoerS3) + `bsp_audio_common_create() + bsp_audio_common_init()`
- close: common 层 stop + PA off + release 资源
- 所有 play/record 函数直接转发到 `bsp_audio_common_*` 同一组函数

参考: `doers3/audio.c` (带 IOEXP), `auras3/audio.c` (direct GPIO).

## 11. gnss.c

实现 `bsp_gnss_get_desc()`, `bsp_gnss_open()`, `bsp_gnss_close()`, `bsp_gnss_read()`.

使用 ESP-IDF UART driver. 通用模式:

- open: `uart_param_config() + uart_set_pin() + uart_driver_install()`
- close: `uart_driver_delete()`
- read: `uart_read_bytes()`
- read 返回 `ESP_OK` 加真实长度;一字节都没读到返回 `ESP_ERR_TIMEOUT` (见 `docs/bsp/design.md` §2)
- 如果 GNSS 需要通过 IOEXP 释放 reset (AuraS3), 在 open 中 acquire IOEXP 并释放 reset, close 中 release

参考: `doers3/gnss.c` (纯 UART), `auras3/gnss.c` (+ IOEXP reset).

## 12. 可选: IOEXP (internal/<board>_ioexp.c/h)

IO expander 用于管理板级控制信号 (LCD CS, PA enable, camera power-down, GNSS reset 等).
每块板自包含实现,不存在跨板复用层.

### 12.1. 自包含实现

每个 IOEXP 是自包含模块,负责:

- 管理 I2C device handle 创建/销毁 (通过 `bsp_i2c_acquire/release` 获取共享 I2C bus)
- 管理 chip driver handle 创建/销毁
- 提供 acquire/release/set_pin/get_pin 四个接口
- 内部 refcount + 线程安全锁

```c
// <board>_ioexp.c — 各板自行实现,直接调用具体芯片 driver,无 vtable 转发层.
esp_err_t <board>_ioexp_acquire(void);   // 获取 IOEXP (refcount++ + 首次初始化)
esp_err_t <board>_ioexp_release(void);   // 释放 IOEXP (refcount--)
esp_err_t <board>_ioexp_set_pin(uint8_t pin, bool level);
```

### 12.2. semantic inline wrappers (internal/<board>_ioexp.h)

在头文件中提供语义函数, 避免调用者接触原始 pin 号:

```c
static inline esp_err_t <board>_ioexp_set_lcd_cs(bool selected) {
    return <board>_ioexp_set_pin(<BOARD>_IOEXP_LCD_CS, !selected);
}
static inline esp_err_t <board>_ioexp_set_audio_pa(bool on) {
    return <board>_ioexp_set_pin(<BOARD>_IOEXP_AUDIO_PA,
                                 on ? <BOARD>_IOEXP_AUDIO_PA_ENABLE_LEVEL
                                    : !<BOARD>_IOEXP_AUDIO_PA_ENABLE_LEVEL);
}
```

参考: `doers3/internal/doers3_ioexp.c/h` (PCA9557), `auras3/internal/auras3_ioexp.c/h` (TCA9554).

## 13. 可选: pmu.c 和 camera.c

### 13.1. PMU

只有有 PMU 硬件的板 (AuraS3 AXP2101) 需要实现. 实现 `bsp_pmu_*` 全部 5 个函数:

- `bsp_pmu_get_desc()`: 返回 `present=true` + model name
- `bsp_pmu_open()`: `bsp_i2c_acquire()` + open AXP2101 + enable ADC/IRQ
- `bsp_pmu_close()`: close AXP2101 + `bsp_i2c_release()`
- `bsp_pmu_get_status()`: 从 AXP2101 读取 status 映射到 `bsp_pmu_status_t`
- `bsp_pmu_get_events()`: 从 AXP2101 读取 event 映射到 `bsp_pmu_event_t`

无 PMU 的板用 `src/common/unsupported/pmu_unsupported.c` (返回 `ESP_ERR_NOT_SUPPORTED`).

参考: `auras3/pmu.c`.

### 13.2. Camera

只有有 camera 硬件且 `CONFIG_BSP_ENABLE_CAMERA=y` 的板 (DoerS3 GC0308) 需要实现. 实现 `bsp_camera_*` 全部 5 个函数:

- `bsp_camera_get_desc()`: 返回 `present=true` + 分辨率/格式
- `bsp_camera_open()`: 初始化 esp32-camera 传感器
- `bsp_camera_capture()`: 采集单帧, 返回 `bsp_camera_frame_t`
- `bsp_camera_release_frame()`: 释放 frame buffer
- `bsp_camera_close()`: 反初始化

无 camera 的板用 `src/common/unsupported/camera_unsupported.c` (返回 `ESP_ERR_NOT_SUPPORTED`).

Camera 是可选 build capability, board.cmake 中通过 `if(CONFIG_BSP_ENABLE_CAMERA)` 条件编译.
启用 camera 的 test_app (如 camera test_app) 还需在 app-local `main/idf_component.yml` 中声明 `espressif/esp32-camera` 依赖,并在 `sdkconfig.defaults` 中设置 `CONFIG_BSP_ENABLE_CAMERA=y`.

参考: `doers3/camera.c`.

## 14. 构建系统

### 14.1. CMakeLists.txt

`components/bsp/CMakeLists.txt` 已经包括:

- common source 列表 (`src/common/bsp_i2c.c`, `src/common/ui.c`, `src/common/sdcard.c`, `src/common/audio.c`, `src/common/lvgl/*`)
- priv_include_dirs 列表 (`src`, `src/common`, `src/common/lvgl`, `src/drivers/*`)
- priv_requires 列表 (`driver`, `esp_lcd`, `esp_timer`, `fatfs`, `freertos`, `sdmmc`);`esp_driver_i2c` 与 `lvgl__lvgl` 在 public `REQUIRES` 中

新增 board 时只需要:

1. 在 CMakeLists.txt 中添加 `CONFIG_BSP_BOARD_<BOARD>` 的 `include` 分支
2. 创建 board.cmake 列出本板特有的文件

### 14.2. Kconfig

需要在 `components/bsp/Kconfig` 中添加:

```
config BSP_BOARD_<BOARD>
    bool "<Board Name>"
```

### 14.3. 验证

board port 完成后,先跑 `tools/check.sh`,再按 `docs/README.md` 的验证入口对每个 app 和每块板构建一遍;所有 test_app 应该能成功构建.

## 15. 关键约束

API 语义,验证和文档规则见 `AGENTS.md` 和 `docs/bsp/design.md`;这里只列 board port 自己的约束:

1. **Board port 只能导出 `bsp_*` public symbols**, 不暴露私有 driver 类型
2. **Linker 选择**, 不运行时区分板型, 不做 board detect
3. **I2C bus 和 IOEXP 通过 `bsp_i2c_acquire/release` 和 `<board>_ioexp_acquire/release` 管理**;board port 内部确有第二处共享资源时 (如 AuraS3 display/backlight 共享 panel), 自己维护 refcount 并说明理由
4. **pins.h 是 pin 分配的单一事实来源**, 代码不硬编码 GPIO 值
5. **修改 board port 后同步 truth table** (`docs/hw/boards/<board>/truth_table.md`): 规则见 `AGENTS.md` §7

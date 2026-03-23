# BSP 设计文档

本文档描述 `components/bsp` 的长期设计边界.当前进度,已完成项和下一步记录在 `docs/bsp/status.md`.

## 1. 目标

- 为 DoerS3 和 AuraS3 提供一组统一 BSP API.
- app 只依赖公开 BSP API,不直接依赖具体开发板的 pin,bus,chip driver 或 power sequence.
- board port 负责消化硬件差异,让同一个 app 可以在不同 board 上使用稳定语义.
- BSP API 保持简单,直接,可解释,优先解决真实需求,不提前抽象.
- BSP 内部代码在不增加复杂度的前提下保持适度复用.

## 2. 非目标

- 不做运行时 board detect.
- 不做大 `boarddb`.
- 不做通用 `bsp_hal_spi/i2c/uart/i2s`;只允许少量 board-owned 诊断 API,例如扫描 BSP 默认 I2C bus.
- 不做运行时 backend registry.
- 不把 BSP 做成应用框架.
- 不承诺暴露每个芯片的完整能力.
- 不把未经确认的实验能力放入稳定公共 API.

## 3. 目录结构

```text
components/
  bsp/
    include/              # app 可见 API
    src/common/           # 真正跨板复用的组合逻辑
    src/boards/doers3/    # DoerS3 board port
    src/boards/auras3/    # AuraS3 board port
    src/drivers/          # BSP 私有 IC driver
    test_app/             # BSP 能力验证工程和 shell 调试 app
  shell/                  # 非 BSP,独立调试组件
```

基本规则:

- `include/` 只放公开 API.
- `src/boards/<board>/` 放板级实现,pin mapping,bus 装配和 power/reset 时序.
- `src/boards/<board>/board.cmake` 直接列出该 board 的 source manifest.
- `src/drivers/` 放可复用但不公开的小芯片 driver.
- `src/common/` 只放真正跨板复用的组合逻辑,不放某块板子的临时 stub.
- `test_app/` 用公开 API 验证 BSP 能力.
- `components/shell` 可以调用 BSP public API 提供调试命令,但 `components/bsp` 不能依赖 shell.

## 4. 公共 API 边界

核心 BSP API 面向 app 稳定使用,不直接暴露底层组件或芯片 driver 类型.

当前规则:

- 核心 API 只暴露 `esp_err_t`,基础 C 类型,BSP 自有 `struct` / `enum`.
- 不在核心 API 中直接暴露 ESP-IDF 或第三方 driver 类型.
- 当前 UI 只支持 LVGL,不设计多 UI backend;允许 `bsp_ui_*` 暴露 LVGL 类型,但函数名必须明确带 `lvgl`.
- 外设生命周期优先使用 `open/close`.
- `open()` 在验证输出指针后必须先将 `*out_handle` / `*out_xxx` 置为 `NULL`;失败返回时调用者不得保留旧 handle.
- `close(NULL)` 是调用参数错误,统一返回 `ESP_ERR_INVALID_ARG`.
- 有描述信息的外设可以提供 `bsp_xxx_get_desc()` 或 `bsp_xxx_get_info()`.
- 有默认配置的外设可以提供 `bsp_xxx_default_config()`.
- IO 操作应返回实际读写长度,并由调用者传入 `timeout_ms`.
- BSP public API 默认不承诺多任务并发安全;同一个 handle 应由一个 owner/task 串行使用.
- 如果 app 需要跨 task 共享同一个 handle,应由 app 自己加锁.
- 不同 handle 是否可以并发使用取决于它们是否共享同一个底层硬件资源;BSP 只保护必要的 board-level shared resource 生命周期,不提供全局并发调度语义.
- API 设计期允许破坏性调整;稳定后新增能力优先 additive.

## 5. Board Port 职责

每块板子实现同一组公开 BSP API,但内部可以完全不同.

board port 负责:

- GPIO / bus / chip address 映射.
- I2C,SPI,I2S,UART,SDMMC/SDSPI 等 bus 生命周期管理.
- power enable,reset,mute,backlight,IOEXP 默认电平等板级时序.
- 多外设共享 bus 或共享控制脚时的引用计数和状态管理.
- 将私有 chip driver 组合成面向 app 的 BSP 语义.
- 声明当前 board port 已接入并可通过公共 API 使用的能力.

board port 不应该:

- 暴露内部头文件给 app.
- 要求 app include 私有 driver.
- 把某块板子的特殊状态机塞进公共 API.
- 为未确认的硬件路径提前设计稳定接口.

## 6. Private Driver 职责

BSP 优先直接管理板上小芯片 driver,避免 app 或 board port 通过 ESP-IDF Component Manager 直接依赖零散外部 driver.

private driver 负责:

- 芯片级寄存器读写.
- 芯片级初始化,格式配置,power/mute/reset 等最小状态控制.
- 暴露足够 board port 使用的小而专接口.

private driver 不负责:

- app 语义.
- board pin 和 bus 选择.
- power sequence 中的板级 GPIO/IOEXP 组合逻辑.
- 复杂通用抽象或跨芯片统一 HAL.

实现策略:

- 可以参考已有开源/托管组件的寄存器序列和基础读写实现.
- 不要求为了"纯净"而完全对照手册从零实现.
- 如果多个 board 真实复用同一芯片 driver,可以在 `src/drivers/` 内复用.
- 不为了假想复用提前设计通用 bus HAL 或 driver framework.
- camera,UI 这类大集成可以保留外部依赖,但公共 API 必须明确边界.

## 7. Common Layer 职责

`src/common` 只放跨板复用的组合层.

适合放入 `src/common` 的内容:

- 只依赖公开 BSP API 的组合逻辑.
- 对 DoerS3 和 AuraS3 都成立的 app-facing 行为.
- 共享 bus owner 或 helper,前提是 board port 仍传入 board truth.
- 不包含具体 GPIO,寄存器地址或板级 power sequence 的代码.

当前 examples:

- `src/common/ui.c`: `bsp_ui` lifecycle,组合 display,touch,backlight 和 LVGL.
- `src/common/bus/bsp_i2c_bus.c`: shared I2C bus owner,由 board wrapper 提供 pins/port/speed.
- `src/common/lvgl/bsp_lvgl_buffer.c`: LVGL double buffer allocation policy.
- `src/common/unsupported/*_unsupported.c`: 当前 board 明确没有或未实现的 capability stub.

不适合放入 `src/common` 的内容:

- 某块板子的 pin,IOEXP,reset,power sequence.
- 尚未确认是否跨板复用的抽象.
- 为了文件数量一致而强行拆出来的 wrapper.

## 8. 外设 API 模式

外设 API 应围绕稳定语义,而不是围绕具体芯片能力设计.

### Display / Backlight

- display API 是低层 board-native async transfer API,不是统一 framebuffer/color helper.
- `bsp_display_open()` 返回 native display handle;`bsp_display_write_async()` 只发送 board-native pixel byte stream.
- `data_size` 只做内存安全校验,不表达 host-order RGB565 语义.
- public display API 不提供 `fill`,`colorbars` 或 host-order writer.
- app UI 应优先使用 `bsp_ui_*`,不要假设不同 board 的 native display byte order 相同.
- backlight API 表达亮度和开关,不暴露具体 PWM,IOEXP 或 panel command.
- backlight 的 `present=false` 和 `get_desc()/open()` 返回 unsupported 由 board port 的 descriptor 和实现直接承担.

### Touch

- touch API 返回 BSP 自有 touch point 结构.
- 不暴露 `esp_lcd_touch_point_data_t`.
- 坐标方向,swap,mirror 等由 board port 消化.

### UI

- `bsp_ui` 是 application UI entry,组合 LVGL display,touch,backlight.
- 当前只支持 LVGL,不准备支持其他 UI backend,因此不做 generic UI vtable 或 backend registry.
- 允许暴露 LVGL 类型,但函数名必须明确带 `lvgl`.
- 因为 LVGL 是唯一 UI backend,`bsp` public component 可以依赖 LVGL;若未来有非 UI app 明显受 build 依赖影响,再拆 `bsp_ui` 子组件.
- `bsp_ui_open()` 复用 `bsp_display_open()`,并与裸 `bsp_display_open()` 互斥.
- `bsp_ui_process()` 不在 BSP 内硬限制 FPS,由 LVGL timer delay 驱动.

### I2C Bus Service

- `bsp_i2c` 是 board-owned public bus service,不是通用 I2C HAL.
- board port 只提供默认 bus truth,例如 port,pin,pull-up,glitch filter 和 recovery 策略.
- core I2C API 通过 board manifest 获取默认 bus ops;common bus 层负责 acquire/release/refcount 和可复用的 bus lifecycle,避免每个外设重复初始化 I2C bus.
- `bsp_i2c_scan()` 和 `bsp_i2c_probe()` 面向 shell/app 诊断,只表达 7-bit address,不允许 app 自己选择 board pin.
- `bsp_i2c_acquire_default()` 是 ESP-IDF escape hatch,用于 app-owned I2C device;调用者必须配对 `bsp_i2c_release_default()`.
- app 若要访问板载芯片,仍应优先使用对应 BSP 外设 API,不要绕过 BSP 直接操作板载 device address.

### SD Card

- sdcard API 表达 mount/unmount 和挂载点.
|- public sdcard API 在 `src/common/sdcard.c` 中由 common 层实现;board port 通过 `bsp_sdcard_get_pins()` 提供 pin 配置.
- 不在核心 API 中暴露 `sdmmc_card_t`.
- board port 可选择 SDMMC 或 SDSPI,但 public API 不暴露 bus 细节.
- 如后续确需访问原生 card handle,应单独设计明确的 ESP-IDF escape hatch.

### GNSS

|- GNSS API 表达串口数据读取,NMEA 数据可用性或后续解析结果.
|- board port 直接实现 `bsp_gnss_*` 公共 API,不存在中间 core 层.
|- UART 选择,波特率默认值和电源/reset 控制由 board port 负责.
|- raw NMEA 输出可以作为早期 test_app / shell 验证路径,稳定结构化定位 API 可后续再收敛.

### IMU

|- IMU API 返回 BSP 自有数据结构.
|- board port 直接实现 `bsp_imu_*` 公共 API,不存在中间 core 层.
|- 字段名应带单位,例如 `accel_mps2_x`,`gyro_rads_x`,`temperature_c`,`timestamp_ms`.
|- 具体芯片寄存器配置留在 private driver 和 board port 内部.

### Touch

|- board port 直接实现 `bsp_touch_*` 公共 API,不存在中间 core 层.
|- 触点坐标和方向校正在 board port 内完成,app 不暴露具体触控芯片配置.

### Display

|- board port 直接实现 `bsp_display_*` 公共 API,不存在中间 core 层.
|- LVGL port 是 internal bridge,由 board display 实现处理 native flush 和 transfer callback.

### Audio

|- audio API 使用最小句柄模型: `open/close` 管资源,`play_*` 管播放,`record_*` 管录音.
|- board port 直接导出 `bsp_audio_*` 公共 API 符号,内部委托给 `src/common/audio.c` 中共享的 I2S+codec 逻辑.
|- board port 通过 `bsp_audio_pins_t` pin 配置结构和 `bsp_audio_pa_fn` PA callback 参数化公共逻辑.
|- playback 和 record 可以共享同一个 handle,但公共 API 不暴露 I2S bus,codec handle 或 slot layout.
|- read/write 返回实际读写长度,并由调用者传入 `timeout_ms`.
|- 当前稳定语义是 ES8311 speaker playback 和 ES7210 MIC1/MIC2 16-bit stereo record.
|- MIC3 playback reference,TDM,AEC 属于暂停的实验方向;未确认前不进入稳定 API.

### Camera

|- camera API 优先提供最小单帧采集: `open -> capture -> release_frame -> close`.
|- DoerS3 board port 直接实现 `bsp_camera_*` 公共 API;AuraS3 的 unsupported 由 `src/common/unsupported/camera_unsupported.c` 提供.
|- frame release 必须带回对应 `bsp_camera_frame_t`,避免多 buffer 时语义不清.
|- AuraS3 无 camera 硬件,必须返回 `ESP_ERR_NOT_SUPPORTED`,且不声明 camera capability.
|- DoerS3 camera 是可选 build capability;只有 `CONFIG_BSP_ENABLE_CAMERA=y` 时才编译 camera port,声明 camera capability 并链接 `esp32-camera` / `esp_jpeg`.
|- 启用 DoerS3 camera 的 app 必须在自己的 `idf_component.yml` 中声明 `espressif/esp32-camera` 和 `espressif/esp_jpeg` 依赖;BSP 默认不强制所有 app 下载和编译 camera 组件.
|- 连续流,JPEG,图像显示,传感器参数调节可以后续增量设计.

### PMU

|- PMU API 第一阶段只表达只读状态和已映射事件: `open` / `close` / `get_status` / `get_events`.
|- AuraS3 board port 直接实现 `bsp_pmu_*` 公共 API;DoerS3 的 unsupported stub 由 `src/common/unsupported/pmu_unsupported.c` 提供.
|- public status 暴露 VBUS,电池,充电,电压和温度,不暴露 AXP2101 raw register 或 TCA9554 raw 电平.
|- 充电参数,DCDC/LDO rail control 和 software power-off 在板级验证前不进入稳定 public API.
|- raw IRQ / raw status 只能作为 bring-up 临时调试手段,结论确认后应删除或留在 internal-only debug,不能进入稳定 public API.

## 9. 错误语义

公共 API 统一使用以下错误含义:

- `ESP_OK`: 操作成功.
- `ESP_ERR_NOT_SUPPORTED`: 当前板子没有该能力,或该 board port 尚未实现.
- `ESP_ERR_INVALID_ARG`: 调用参数错误.
- `ESP_ERR_INVALID_STATE`: 生命周期或状态错误,例如未 open,重复 start,未 mount.
- `ESP_ERR_TIMEOUT`: 带 timeout 的操作等待超时.
- `ESP_ERR_NOT_FOUND`: 硬件按设计应存在,但探测不到或无响应.
- 其他 ESP-IDF 错误码可以向上传递,但 board port 应尽量在日志中说明上下文.

## 10. 并发和状态边界

BSP public API 默认按简单串行模型设计,不把每个外设 API 都做成线程安全接口.

- `handle` 指一次 `bsp_xxx_open()` 返回的资源实例,例如一个 audio/display/touch/camera/sdcard handle.
- 同一个 handle 应由一个 owner/task 串行调用;如果多个 task 共享同一个 handle,app 必须自己加 mutex.
- 不同 handle 可以在 app 层分属不同 task,但如果它们共享底层硬件资源,仍应避免无序并发访问.
- BSP board port 应保护必要的 shared board resource 生命周期,例如 shared I2C bus,IO expander,power enable,reset,PA mute,display transfer callback 等.
- BSP 不提供全局 bus scheduler,不承诺 open/close/read/write/capture 在跨 task 乱序调用时仍安全.
- close/start/stop 这类生命周期 API 应尽量检查状态,返回 `ESP_ERR_INVALID_STATE`,并在失败路径做必要回滚.

建议 app 使用方式:

- 一个外设 handle 由一个 task 拥有并负责 open/close.
- 其他 task 通过 queue/event/message 请求该 owner task 操作外设.
- 如果必须直接共享 handle,在 app 层围绕该 handle 加锁.

## 11. Kconfig

Kconfig 选择当前 board,以及少量重量级 capability 的 build 开关:

```text
CONFIG_BSP_BOARD_DOERS3
CONFIG_BSP_BOARD_AURAS3
CONFIG_BSP_ENABLE_CAMERA
```

- 不在 Kconfig 中配置每个 GPIO,屏幕尺寸,UART 口,I2C 地址或 codec 路径.
- 可以用 Kconfig 选择重量级可选 capability 的编译开关,例如 `CONFIG_BSP_ENABLE_CAMERA`,用于避免非 camera app 强制链接 camera 依赖.

原因:

- 这些属于 board truth,不应变成 app 配置项.
- 配置项过多会让 BSP 变成半成品 board database.
- app 只应选择目标 board,而不是重新描述硬件.

## 12. Test App 规则

每个 BSP 能力都应放在 `components/bsp/test_app/<name>` 下单独验证.

规则:

- test_app 只依赖公开 `bsp` API.
- test_app 不 include board port 或 private driver 头文件.
- 构建入口统一使用 `components/bsp/test_app/bsp.sh`.
- 每个 app 只维护一个 `sdkconfig.defaults`,一个真实 `sdkconfig`,一个 `build/`.
- board 选择由 wrapper 写入 app-local `sdkconfig`.
- 同一个 app 切换 board 时,wrapper 自动清理 `sdkconfig` 和 `build/`.
- 需要硬件动作的测试在日志中明确提示,不把人工步骤藏进 BSP 代码.
- 无法自动判定的测试应输出足够数据,供人工判断.
- 删除或暂停的实验能力不保留长期 test_app 噪声.

常用命令:

```bash
cd components/bsp/test_app
./bsp.sh board aura build flash monitor
./bsp.sh ui doer build flash monitor
```

## 13. Shell 调试边界

shell 用于手动 bring-up/debug,不是 BSP public API 的替代品.

当前原则:

- shell 命令只调用 BSP public API.
- `i2c scan` 这类诊断命令也应走 BSP public diagnostic API,避免 shell 复制 board pin/port.
- shell 可用于 `bsp`, `imu`, `touch`, `gnss`, `backlight`, `sd`, `audio` 等手动调试.
- shell 不恢复 display `fill` / `colorbars` 命令,避免把 board-native display byte order 变成应用语义.
- 若某个调试能力需要 board-private hook,应先讨论是否值得进入 public API 或 test_app,不要直接让 shell include board private header.

## 14. 演进策略

- 设计期允许破坏性调整 API,优先把接口形态做对.
- 稳定后新增能力优先 additive,避免无意义 churn.
- 先实现真实需要的最小能力,再按实际需求扩展.
- 未确认硬件事实先写入 truth table 的待确认部分,不进入稳定 API.
- 实验能力优先放在内部实现或临时 test_app 中验证,确认后再设计公共 API.
- 如果抽象让代码更难解释,调试或验证,应退回更直接的实现.

## 15. 文档分工

- `docs/bsp/README.md`: BSP 文档入口和导航.
- `docs/bsp/porting-guide.md`: Board port 接入指南和每个文件的实现模板.
- `docs/bsp_design.md`: 长期设计边界和 API 原则.
- `docs/bsp/status.md`: 当前状态,已完成项,缺口和下一步.
- `docs/hw/boards/*_truth_table.md`: 硬件事实,pin,bus,芯片连接和待确认项.
- `docs/hw/auras3-display-te.md`: AuraS3 CO5300 TE 实验记录和当前取舍.
- `docs/hw/auras3-pmu-key.md`: AuraS3 AXP2101 PMU,KEY2,SYS_OUT 和 AXP_IRQ 阶段性结论.
- `AGENTS.md`: 给 Codex / coding agent 的协作规则.

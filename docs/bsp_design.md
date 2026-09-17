# BSP 设计文档

本文档描述 `components/bsp` 的结构,分层职责和 API 语义约定.协作和架构规则见 `AGENTS.md`;当前进度,已完成项和下一步记录在 `docs/bsp/status.md`.

## 1. 目录结构

```text
components/
  bsp/
    include/                # app 可见 API
    src/common/             # 真正跨板复用的组合逻辑
    src/common/unsupported/ # 能力缺席时的共享默认实现
    src/boards/doers3/      # DoerS3 board port
    src/boards/auras3/      # AuraS3 board port
    src/drivers/            # BSP 私有 IC driver
    test_app/               # BSP 能力验证工程和 shell 调试 app
  shell/                    # 非 BSP,独立调试组件
```

基本规则:

- `include/` 只放公开 API.
- `src/boards/<board>/` 放板级实现,pin mapping,bus 装配和 power/reset 时序.
- `src/boards/<board>/board.cmake` 声明该 board 实现的能力及其 source manifest;未声明的能力由 CMake 兜底到 `src/common/unsupported/`.
- `src/drivers/` 放可复用但不公开的小芯片 driver.
- `src/common/` 只放真正跨板复用的组合逻辑,不放某块板专用的临时实现.
- `src/common/unsupported/` 放能力缺席时的共享默认实现 (`desc.present = false`,API 返回 `ESP_ERR_NOT_SUPPORTED`),每个可能缺席的公开能力至多一份;不允许"既无实现又无兜底"的能力留到链接期.
- `test_app/` 用公开 API 验证 BSP 能力.
- `components/shell/` 非 BSP 的独立调试组件,不属于 BSP 交付面.

## 2. 公共 API 语义约定

公共 API 的通用规则见 `AGENTS.md` §3;本节只记录 API 的具体语义约定.

- 当前 UI 只支持 LVGL,不设计多 UI backend;允许 `bsp_ui_*` 暴露 LVGL 类型,但函数名必须明确带 `lvgl`.
- I2C bus 是另一处具名例外: `bsp_i2c_acquire()` 返回原生 `i2c_master_bus_handle_t`,公开它的理由是防止 app 在同一组 pin 上另开第二条 bus. bus 的 port,pin 和参数属于 board truth,留在 `src/common/bsp_i2c_internal.h`.
- `open()` 在验证输出指针后必须先将 `*handle_out` / `*xxx_out` 置为 `NULL`;失败返回时调用者不得保留旧 handle;同一外设重复 `open()` 返回 `ESP_ERR_INVALID_STATE`,不隐式返回第二个 handle.
- `open()` 失败必须回滚到未打开状态.
- `close(NULL)` 是调用参数错误,统一返回 `ESP_ERR_INVALID_ARG`;`handle` 在 `close()` 后立即失效,不承诺重复 `close()` 幂等.
- `start` / `stop` 是幂等运行态操作,重复调用返回 `ESP_OK`;生命周期严格性只针对 `open` / `close`.
- 有描述信息的外设可以提供 `bsp_xxx_get_desc()` 或 `bsp_xxx_get_info()`.
- 有默认配置的外设可以提供 `bsp_xxx_default_config()`.
- IO 操作应返回实际读写长度,并由调用者传入 `timeout_ms`;长度参数 (`*xxx_out`) 只要指针合法就总是有效,失败返回时也写入真实传输量.
- 带 `timeout_ms` 的阻塞读写: 传输了任何字节就返回 `ESP_OK` (短读/短写不是错误);一个字节都没传成才返回 `ESP_ERR_TIMEOUT`,长度参数写 0.

## 3. Board Port 职责

每块板子实现同一组公开 BSP API,但内部可以完全不同. 通用约束见 `AGENTS.md` §4.

board port 负责:

- GPIO / bus / chip address 映射,以及 I2C,SPI,I2S,UART,SDMMC 等 bus 的生命周期.
- power enable,reset,mute,backlight,IOEXP 默认电平等板级时序.
- 多外设共享 bus 或共享控制脚时的引用计数和状态管理.
- 将私有 chip driver 组合成面向 app 的 BSP 语义.
- 声明本板接入的能力: `board.cmake` 登记实现,缺口登记到 `BSP_BOARD_UNSUPPORTED` 由 CMake 兜底;`desc.present` 与实现一致.

board port 的实现细节只存在于 `src/boards/<board>/` 内,不进入公共 API.

## 4. Private Driver 职责

`src/drivers/` 放 BSP 私有小芯片 driver,通用规则见 `AGENTS.md` §4.

driver 与 board port 的分工:

- driver 只处理芯片级事实: 寄存器读写,初始化序列,格式配置,power/mute/reset 等最小状态控制.
- driver 不感知板级事实: pin,bus 选择,地址,power sequence 中的 GPIO/IOEXP 组合都由 board port 传入.
- driver 不承载 app 语义,不为了跨芯片统一做抽象.

## 5. Common Layer 职责

`src/common` 放跨板复用的组合层: 对 DoerS3 和 AuraS3 都成立的行为才放这里.

- 组合逻辑可以依赖 BSP 内部头和私有 driver,但 board truth (pin,bus,地址,板级时序) 只能由 board port 以 `get_config()` / `get_pins()` 这类形式提供.
- 适合: 共享 bus owner,共享 helper,组合多个外设的 app-facing 行为.
- 不适合: 某块板子的 pin/IOEXP/reset/power sequence,尚未确认跨板复用的抽象,为凑文件数强行拆出的 wrapper.

当前文件 (细节见源码):

- `bsp_i2c.c`: shared I2C bus owner (singleton + refcount),配置来自 board port 的 `bsp_i2c_get_config()`.
- `ui.c`: `bsp_ui` lifecycle,组合 display,touch,backlight 和 LVGL.
- `audio.c`: 基于私有 ES8311/ES7210 driver 的播放/录音组合逻辑.
- `sdcard.c`: 挂载/VFS 逻辑,pin 来自 board port 的 `bsp_sdcard_get_pins()`.
- `lvgl/`: LVGL port 与 double buffer 分配策略.
- `unsupported/`: 能力缺席时的共享 stub,由 CMake 按 `BSP_BOARD_UNSUPPORTED` 兜底.

## 6. 外设 API 模式

外设 API 应围绕稳定语义,而不是围绕具体芯片能力设计.

### Display / Backlight

- display API 是低层 board-native async transfer API,不是统一 framebuffer/color helper.
- `bsp_display_open()` 返回 native display handle;`bsp_display_write()` 只发送 board-native pixel byte stream.
- `data_size` 只做内存安全校验,不表达 host-order RGB565 语义.
- public display API 不提供 `fill`,`colorbars` 或 host-order writer.
- app UI 应优先使用 `bsp_ui_*`,不要假设不同 board 的 native display byte order 相同.
- board port 直接实现 `bsp_display_*` 公共 API,不存在中间 core 层.
- LVGL port 是 internal bridge,由 board display 实现处理 native flush 和 transfer callback.
- backlight API 表达亮度和开关,不暴露具体 PWM,IOEXP 或 panel command.

### Touch

- touch API 返回 BSP 自有 `bsp_touch_point_t`,不暴露 `esp_lcd_touch_point_data_t` 等第三方类型.
- 坐标方向,swap,mirror 和触点校正由 board port 消化,app 不感知具体触控芯片配置.
- board port 直接实现 `bsp_touch_*` 公共 API,不存在中间 core 层.

### UI

- `bsp_ui` 是 application UI entry,组合 LVGL display,touch,backlight.
- 当前只支持 LVGL,不准备支持其他 UI backend,因此不做 generic UI vtable 或 backend registry.
- 允许暴露 LVGL 类型,但函数名必须明确带 `lvgl`.
- 因为 LVGL 是唯一 UI backend,`bsp` public component 可以依赖 LVGL;若未来有非 UI app 明显受 build 依赖影响,再拆 `bsp_ui` 子组件.
- `bsp_ui_open()` 复用 `bsp_display_open()`,并与裸 `bsp_display_open()` 互斥.
- `bsp_ui_process()` 不在 BSP 内硬限制 FPS,由 LVGL timer delay 驱动.

### I2C Bus Service

- `bsp_i2c` 是 board-owned public bus service,不是通用 I2C HAL.
- board port 通过 `bsp_i2c_get_config()` 提供默认 bus truth (port,pin,pull-up,glitch filter);结构体和声明在内部头 `src/common/bsp_i2c_internal.h`,不进入公开 API.
- `src/common/bsp_i2c.c` 是单例 bus owner,负责 acquire/release/refcount/I2C 创建和销毁,避免每个外设重复初始化 I2C bus.
- `bsp_i2c_scan()` 和 `bsp_i2c_probe()` 面向 app 诊断,只表达 7-bit address,不允许 app 自己选择 board pin.
- `bsp_i2c_probe()` 和 `bsp_i2c_scan()` 自带 acquire/release,调用者不需要先 acquire.
- `bsp_i2c_acquire()` 是唯一入口: 首次调用时创建 bus,后续递增 refcount. 调用者必须配对 `bsp_i2c_release()`.
- `bsp_i2c_acquire()` 是 public API 中唯一暴露 ESP-IDF 类型的逃生舱,供 app 挂接自己的 I2C 器件;不要为一个便利接口再扩大这个例外.
- app 若要访问板载芯片,仍应优先使用对应 BSP 外设 API,不要绕过 BSP 直接操作板载 device address.

### SD Card

- sdcard API 表达 handle 生命周期,mount/unmount,挂载点和只读的 card / FAT 信息查询.
- public sdcard API 在 `src/common/sdcard.c` 中由 common 层实现;board port 通过 `bsp_sdcard_get_pins()` 提供 pin 配置.
- 不在核心 API 中暴露 `sdmmc_card_t`.
- `open()` 只分配 handle,不访问硬件;SDMMC 初始化和卡检测在 `mount()` 时发生,`close()` 会自动 unmount. 挂载点必须是以 `/` 开头的绝对路径.
- 当前实现固定为 SDMMC 1-bit,board port 只提供 `clk/cmd/d0` 三个 pin. 若未来需要 SDSPI 或 4-bit,先扩展 board 配置结构,再由 common 层支持.
- 如后续确需访问原生 card handle,应单独设计明确的 ESP-IDF escape hatch.

### GNSS

- GNSS API 只表达串口原始字节流读取;NMEA 分帧和坐标解析不属于 BSP,由 app 完成.
- board port 直接实现 `bsp_gnss_*` 公共 API,不存在中间 core 层.
- UART 选择,波特率默认值和电源/reset 控制由 board port 负责.
- `bsp_gnss_read()` 在超时内读到数据就返回 `ESP_OK` 加实际长度 `len_out`;一个字节都没读到返回 `ESP_ERR_TIMEOUT`,`len_out == 0`.

### IMU

- IMU API 返回 BSP 自有数据结构.
- board port 直接实现 `bsp_imu_*` 公共 API,不存在中间 core 层.
- 字段名应带单位,例如 `accel_mps2_x`,`gyro_rads_x`,`temperature_c`;单位未确认的值不带 `_ms` / `_us` 之类的时间后缀,例如 `timestamp_ticks`.
- 具体芯片寄存器配置留在 private driver 和 board port 内部.
- 当前读路径是轮询数据寄存器,不使用 FIFO 或中断;`bsp_imu_is_data_ready()` 供 app 自行决定采样节奏.

### Audio

- audio API 使用最小句柄模型: `open/close` 管资源,`play_*` 管播放,`record_*` 管录音.
- board port 直接导出 `bsp_audio_*` 公共 API 符号,内部委托给 `src/common/audio.c` 中共享的 I2S+codec 逻辑.
- board port 通过 `bsp_audio_pins_t` pin 配置结构和 `bsp_audio_pa_fn` PA callback 参数化公共逻辑.
- playback 和 record 可以共享同一个 handle,但公共 API 不暴露 I2S bus,codec handle 或 slot layout.
- read/write 返回实际读写长度,并由调用者传入 `timeout_ms`.
- 当前承诺的 playback / record 路径见 `docs/bsp/capabilities.md`;MIC3 playback reference,TDM,AEC 和 full-duplex 未确认真机行为前不进入稳定 API.

### Camera

- camera API 优先提供最小单帧采集: `open -> capture -> release_frame -> close`.
- 同一时刻只允许一个 active frame;`capture()` 在上一帧 `release_frame()` 之前会返回错误,且可能阻塞等待新帧.
- DoerS3 board port 直接实现 `bsp_camera_*` 公共 API;AuraS3 的 unsupported 由 `src/common/unsupported/camera_unsupported.c` 提供.
- frame release 必须带回对应 `bsp_camera_frame_t`,避免多 buffer 时语义不清.
- AuraS3 无 camera 硬件,必须返回 `ESP_ERR_NOT_SUPPORTED`,且不声明 camera capability.
- DoerS3 camera 是可选 build capability;只有 `CONFIG_BSP_ENABLE_CAMERA=y` 时才编译 camera port,声明 camera capability 并链接 `esp32-camera` / `esp_jpeg`.
- 启用 DoerS3 camera 的 app 只需在自己的 `idf_component.yml` 中声明 `espressif/esp32-camera`;`esp_jpeg` 由它的必需依赖自动带入. BSP 默认不强制所有 app 下载和编译 camera 组件.
- 连续流,JPEG,图像显示,传感器参数调节可以后续增量设计.

### PMU

- PMU API 第一阶段只表达只读状态和已映射事件: `open` / `close` / `get_status` / `get_events`.
- AuraS3 board port 直接实现 `bsp_pmu_*` 公共 API;DoerS3 的 unsupported stub 由 `src/common/unsupported/pmu_unsupported.c` 提供.
- public status 暴露 VBUS,电池,充电,电压和温度,不暴露 AXP2101 raw register 或 TCA9554 raw 电平.
- raw IRQ / raw status 只能作为 bring-up 临时调试手段,结论确认后应删除或留在 internal-only debug,不能进入稳定 public API.

## 7. 错误语义

公共 API 统一使用以下错误含义:

- `ESP_OK`: 操作成功.
- `ESP_ERR_NOT_SUPPORTED`: 当前板子没有该能力,或该 board port 尚未实现.
- `ESP_ERR_INVALID_ARG`: 调用参数错误.
- `ESP_ERR_INVALID_STATE`: 生命周期或状态错误,例如未 open,重复 open,未 start,未 mount.
- `ESP_ERR_TIMEOUT`: 带 timeout 的阻塞读写在超时内没有传输任何字节,或内部等待 (锁,I2C 总线) 超时.
- `ESP_ERR_NOT_FOUND`: 硬件按设计应存在,但探测不到或无响应.
- 其他 ESP-IDF 错误码可以向上传递,但 board port 应尽量在日志中说明上下文.

## 8. 并发和状态边界

BSP public API 默认按简单串行模型设计,不把每个外设 API 都做成线程安全接口.

- `handle` 指一次 `bsp_xxx_open()` 返回的资源实例,例如一个 audio/display/touch/camera/sdcard handle.
- 同一个 handle 应由一个 owner/task 串行调用;如果多个 task 共享同一个 handle,app 必须自己加 mutex.
- 不同 handle 可以在 app 层分属不同 task,但如果它们共享底层硬件资源,仍应避免无序并发访问.
- BSP board port 应保护必要的 shared board resource 生命周期,例如 shared I2C bus,IO expander,power enable,reset,PA mute,display transfer callback 等.
- BSP 不提供全局 bus scheduler,不承诺 open/close/read/write/capture 在跨 task 乱序调用时仍安全.
- `close` 等生命周期 API 应检查状态并返回 `ESP_ERR_INVALID_STATE`,在失败路径做必要回滚;`start` / `stop` 幂等 (见 §2).
- `bsp_display_set_done_cb()` 应在传输开始前设置;回调运行在中断上下文,内部不得阻塞.
- LVGL 不是线程安全的: app 的 LVGL 调用必须与 `bsp_ui_process()` 在同一 task,或由 app 自己加锁;BSP 当前不提供 ui lock.

建议 app 使用方式:

- 一个外设 handle 由一个 task 拥有并负责 open/close.
- 其他 task 通过 queue/event/message 请求该 owner task 操作外设.
- 如果必须直接共享 handle,在 app 层围绕该 handle 加锁.
- 需要从其他 task 操作 UI 时,通过 queue/event 交给 UI task;不要直接跨 task 调 LVGL.

## 9. Kconfig

Kconfig 选择当前 board,以及少量重量级 capability 的 build 开关:

```text
CONFIG_BSP_BOARD_DOERS3
CONFIG_BSP_BOARD_AURAS3
CONFIG_BSP_ENABLE_CAMERA
```

- 不在 Kconfig 中配置每个 GPIO,屏幕尺寸,UART 口,I2C 地址或 codec 路径.
- 可以用 Kconfig 选择重量级可选 capability 的编译开关,例如 `CONFIG_BSP_ENABLE_CAMERA`,用于避免非 camera app 强制链接 camera 依赖.
- board choice 默认 DoerS3;test_app 通过 `bsp.sh` 显式写入,不依赖默认值.

原因:

- 这些属于 board truth,不应变成 app 配置项.
- 配置项过多会让 BSP 变成半成品 board database.
- app 只应选择目标 board,而不是重新描述硬件.

## 10. Test App 规则

验证范围划分 (哪些能力进 test_app,哪些合并到 shell) 和 test_app 的依赖边界见 `AGENTS.md` §6;本节只列工程机制.

规则:

- 构建入口统一使用 `components/bsp/test_app/bsp.sh`.
- 每个 app 只维护一个 `sdkconfig.defaults`,一个真实 `sdkconfig`,一个 `build/`.
- board 选择由 wrapper 写入 app-local `sdkconfig`.
- 同一个 app 切换 board 时,wrapper 自动清理 `sdkconfig` 和 `build/`.
- 需要硬件动作的测试在日志中明确提示,不把人工步骤藏进 BSP 代码.
- 无法自动判定的测试应输出足够数据,供人工判断.
- 需要额外 managed component 的 app 在自己的 `main/idf_component.yml` 声明依赖,例如 camera app 的 `espressif/esp32-camera`.
- 删除或暂停的实验能力不保留长期 test_app 噪声.

验证入口命令见 `docs/bsp/README.md`.

## 11. Shell 调试边界

shell 用于手动 bring-up/debug,不是 BSP public API 的替代品.

当前原则:

- shell 命令只调用 BSP public API.
- `i2c scan` 这类诊断命令也应走 BSP public diagnostic API,避免 shell 复制 board pin/port.
- 当前命令清单和用法见 `components/shell/README.md`.
- shell 不恢复 display `fill` / `colorbars` 命令,避免把 board-native display byte order 变成应用语义.
- 若某个调试能力需要 board-private hook,应先讨论是否值得进入 public API 或 test_app,不要直接让 shell include board private header.

## 12. 演进策略

- 设计期允许破坏性调整 API,优先把接口形态做对.
- 稳定后新增能力优先 additive,避免无意义 churn.
- 先实现真实需要的最小能力,再按实际需求扩展.
- 未确认的硬件事实不进入稳定 API;记录位置和同步规则见 `AGENTS.md` §6 和 §7.
- 实验能力优先放在内部实现或临时 test_app 中验证,确认后再设计公共 API.
- 如果抽象让代码更难解释,调试或验证,应退回更直接的实现.

## 13. 文档分工

- 根 `README.md`: 仓库目的,构成和快速开始;文档索引入口只写 `docs/bsp/README.md`.
- `docs/bsp/README.md`: 全仓库文档和代码入口索引.
- `docs/bsp/capabilities.md`: 能力承诺的唯一出处 — 双板能力矩阵,public API 一览,已承诺和不承诺边界.
- `docs/bsp/status.md`: 进度面 — 已完成,未验证,暂停和下一步;不重复承诺边界.
- `docs/bsp_design.md`: BSP 结构,分层职责和 API 语义.
- `docs/bsp/porting-guide.md`: board port 接入指南和每个文件的实现模板.
- `docs/hw/boards/*_truth_table.md`: 板级硬件事实,pin,bus,芯片连接,待确认项和该板的验证记录.
- `docs/hw/auras3-display-te.md`: AuraS3 CO5300 TE 实验记录和当前取舍.
- `docs/hw/auras3-pmu-key.md`: AuraS3 AXP2101 PMU,KEY2,SYS_OUT 和 AXP_IRQ 阶段性结论.
- `components/shell/README.md`: 调试 shell 使用说明.
- `AGENTS.md`: 给 Codex / coding agent 的协作规则;架构禁项 (no runtime board detect, `boarddb`,通用 HAL) 以 §3 为准.

同一事实只在一个文档里定义: 能力边界看 capabilities,进度看 status,设计规则看本文档和 `AGENTS.md`,硬件事实看 truth table. 其他文档需要时写链接,不复制内容.

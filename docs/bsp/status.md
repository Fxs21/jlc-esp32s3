# BSP 当前状态

本文档只记录当前要做什么、做了什么、没做什么和下一步。

## 要做什么

- 将分散的 `bsp_*` 组件收敛为一个公开组件 `components/bsp`。
- 公开 API 保持统一，板级实现按 `DoerS3` / `AuraS3` 分离。
- 先把 UI 相关路径跑顺：`display -> touch -> backlight -> ui`。
- IOEXP 只作为 DoerS3 内部基础设施，不暴露给 app 或通用 BSP API。
- AuraS3 当前仍允许是占位实现，返回 `ESP_ERR_NOT_SUPPORTED` 是预期行为。

## 已完成

- 新增单一公开 BSP 组件：`components/bsp`。
- Kconfig 板子选择移动到 `components/bsp/Kconfig`。
- 公共头文件集中到 `components/bsp/include`。
- DoerS3 已有独立 board port：display、touch、backlight、sdcard、gnss、imu、audio。
- AuraS3 已有独立占位 board port：display、touch、backlight、sdcard、gnss、imu、audio。
- DoerS3 IOEXP/PCA9557/I2C 已退到 DoerS3 内部实现，不再作为公开组件。
- Display 已改为板级 port 直接装配 ST7789；DoerS3 的 LCD CS 只在 open/close 生命周期控制。
- `bsp_ui` 作为公共组合层保留，只调用 display/touch/backlight 公共 API。
- 旧的分散 `bsp_*` 组件已从 `components/` 移出，避免继续作为 ESP-IDF 组件暴露。
- UI、board、sdcard、gnss、touch、imu、audio test_app 已迁移到 `components/bsp/test_app/`。
- 根应用不再依赖 shell；shell 只作为独立调试组件保留。
- `components/x-track` 已移出 `components/`，当前放在 `_external/x-track`，避免被 ESP-IDF 当作组件扫描。
- DoerS3 IMU 已接入 QMI8658，使用公共 I2C 内部基础设施，输出加速度、陀螺仪、温度。
- DoerS3 Audio 已接入 ES8311/ES7210、I2S 和 PCA9557 上的 PA_EN。
- Audio API 已拆成 playback 与 record 两组控制，保留一个 `bsp_audio_handle_t`。
- 未实现外设的占位实现已收敛到各自 board port；`src/common` 只保留真正跨板复用的 `ui` 组合层。
- Camera 只有占位且当前无 test_app/真实实现，已先从 BSP 公开 API 中移除；后续接入 GC0308 时再加回来。
- 根目录 `idf.py build` 已通过。
- AuraS3 占位配置下 `components/bsp/test_app/board` 已通过构建。
- `components/bsp/test_app/board`、`sdcard`、`gnss`、`touch`、`imu`、`audio` 均已通过 `idf.py -C ... build`。
- DoerS3 真机已确认：board 信息正常，UI 显示和触摸正常，touch test_app 正常，SD 卡可挂载，IMU 读数正常。
- DoerS3 GNSS 使用 MAX-M10S，`UART1` 在 `38400 8N1` 下已确认能收到 NMEA ASCII。

## 没做什么

- Camera 尚未迁移真实硬件实现；当前不暴露 `bsp_camera` API。
- AuraS3 真实硬件参数尚未补齐，UI 不能真正运行。
- GNSS 当前只验证到能收到 NMEA；输出显示 `V` / fix quality `0` / satellites `00`，定位成功仍待室外或良好天线环境确认。
- Audio 已真机确认播放有声音、录音有输入动态；退出清理仍需复测确认。
- Camera test_app 尚未迁移，需等真实 GC0308 接入时一起补。
- 没有引入通用 `bsp_ctrl_pin`、`bsp_hal_*` 或运行时 driver registry。

## 下一步

1. 复测 Audio：确认拆分后的 playback/record API 正常，并确认退出时无 I2S channel 清理错误。
2. 继续确认 GNSS 定位：等待有效 RMC/GGA，确认经纬度、卫星数和 fix quality。
3. 接入 GC0308 时重新增加 `bsp_camera` API 和 Camera test_app。
4. 为 AuraS3 补齐真实 display/touch/backlight/power 信息。
5. 继续保持 shell 在 BSP 之外，不进入 BSP 依赖链。

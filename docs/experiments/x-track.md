# X-TRACK 移植实验

X-TRACK 是一个 LVGL 骑行码表应用. 本文记录把它移植到 DoerS3 / AuraS3 的过程和约定.

范围: 本文只记录移植本身,自成一体,不进入仓库的文档索引,也不代表仓库已确定的应用方向. 是否作为正式承载应用尚未决定.

## 1. 分层约定

X-TRACK 上游代码分成 `App` 和 `HAL` 两部分,移植时保持同样的分层:

| 层 | 位置 | 规则 |
|---|---|---|
| App (业务层) | `components/x-track/App/` | 与上游 `xtrack-app` 仓库保持一致,改动越少越好 |
| HAL (平台层) | `components/x-track/HAL/` | 本仓库自己实现,按平台差异适配 |

- App 层不做平台判断,平台差异全部在 HAL 层消化.
- HAL 层只使用 BSP public API,不 include board port 或 BSP 私有 driver 头文件.

## 2. 集成方式

- `components/x-track/App` 是 git submodule,指向 `git@github.com:Fxs21/xtrack-app.git`.
- `components/x-track/CMakeLists.txt` (在 `x-track` 分支上) 注册 App + HAL 源码,`REQUIRES bsp esp_timer lvgl`.
- 上游 App 的 include 写法是 `lvgl/lvgl.h`,组件在 build 目录生成一层兼容头映射到官方 `lvgl.h`,避免修改上游源码.
- 应用入口在 `main/main.cpp`,顺序是 `HAL::HAL_Init()` -> `App_Init()` -> 循环 `HAL::HAL_Update()`.

## 3. HAL 当前覆盖

`components/x-track/HAL/`:

```text
HAL.cpp            HAL 初始化,更新和模块组装
HAL_Audio.cpp      播放/录音
HAL_Buzz.cpp       蜂鸣器
HAL_Clock.cpp      时间
HAL_Encoder.cpp    旋钮编码器
HAL_GPS.cpp        GNSS
HAL_IMU.cpp        加速度/陀螺
HAL_MAG.cpp        磁力计
HAL_Power.cpp      电源和电量
HAL_SD_CARD.cpp    SD 卡和文件系统
TinyGPSPlus/       NMEA 解析
```

## 4. 分支

| 分支 | 内容 |
|---|---|
| `master` | BSP 主线: `components/bsp`, `components/shell` 和文档 |
| `x-track` | X-TRACK 移植实现: `components/x-track/`, `main/main.cpp`,submodule 声明 |

移植实现以 `x-track` 分支为准. master 工作区里的 `components/x-track/App` 可能只是一个未跟踪的本地 checkout,不要当作已入库内容.

## 5. 待办

- 决定这个移植是否继续,以及是否作为正式承载应用.
- 明确 `components/x-track/App` 的入库方式 (submodule 或 vendor),避免把内层 `.git` 直接提交成 gitlink.
- 真机验证 App 在 DoerS3 / AuraS3 上的完整功能路径.
- 确认 HAL 每个模块背后依赖的 BSP 能力都是 public API,不绕过 BSP 直接访问硬件.

## 6. 参考

- 双板能力: `docs/bsp/capabilities.md`
- 硬件事实: `docs/hw/boards/doers3/truth_table.md`, `docs/hw/boards/auras3/truth_table.md`

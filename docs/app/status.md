# xtrack 当前状态

本文档只记录 App 层的进度面: 已完成, 已知限制和下一步. 结构和边界见 `docs/app/design.md`;BSP 进度见 `docs/bsp/status.md`.

## 已完成

- 并入本仓库 (subtree 保留完整历史), 布局定案: `xtrack/` (App 层) + `sim/` (PC 后端) + `main/` (板侧后端);`components/` 只保留 `bsp` 和 `shell`.
- PC 模拟器可编可跑 (2026-10-14): LVGL 9.5.0 (submodule `85aa60d`) + SDL2,466x466 圆屏,启动页 -> 表盘页流程正常.
- root 工程 `idf.py build` 通过: App 层以 IDF 组件 `xtrack` 编入, 板侧运行逻辑未接入.
- App 字体开关已对齐两侧 (`sdkconfig.defaults` 开 14/16/32/48, `sim/lv_conf.h` 对应开启).

## 已知限制

- 板侧未接入: `main/main.c` 还是空入口,板侧 HAL (clock / gps / power) 和 UI 接线都没有;在此之前 App 只在 PC 模拟器上验证.
- GPS 数据链断裂: `hal_gps_get_info()` 在 App 内没有调用者,`dp_gps.c` 自己造模拟轨迹;板侧接入前先定案 (候选: HAL 只搬 NMEA 字节, 解析放 App 层, PC 端喂罐头数据).
- 布局底稿按 480x320 横屏设计, 现在模拟器和 AuraS3 都是 466x466 圆屏: 圆屏内接安全区只有约 330x330, `pages/status_bar` 的全宽条和 `pages/dialplate` 的上下数据带会被裁, 需要按圆屏适配;DoerS3 320x240 的适配同样待做.
- 输入模型: PC 是鼠标拖动 + 滚轮 (滚轮映射成 encoder 做组内焦点导航), 板子是电容触摸, 没有编码器;拖动返回 (`pm_root_enable_drag()`) 目前没有任何页面启用, 阈值和轴向待实测定案 (轴向由全局动画方向推出, 当前是纵向).
- 板侧 HAL 目前只会是模拟数据 (PC 侧 `sim/hal/` 已经是), 真实 BSP 能力 (GNSS / PMU / RTC) 未接入.
- `sim/main.c` 的圆形遮罩和截图通道 (`PROTO_SHOT`) 是临时脚手架, 布局定案后删除.

## 下一步

1. 板侧接入: `main/` 里用 BSP 打开 UI,调 `hal_init()` 和 `app_init()`,再进 `bsp_ui_process()` 循环;先 DoerS3 后 AuraS3.
2. GPS HAL 缝定案并实现 (见已知限制).
3. 圆屏布局适配, 真机触摸验证和拖动返回阈值定案.
4. 后端跑通后清掉 `sim/main.c` 的布局脚手架.

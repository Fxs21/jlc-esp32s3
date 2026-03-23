# AGENTS Instructions

本文件是给 Codex 的任务约束，不是对外用户文档。

## 1) 评审与设计原则

1. 第一步永远是简化数据结构
2. 消除所有特殊情况
3. 用最笨但最清晰的方式实现
4. 确保零破坏性

## 2) 当前任务范围

- 目标：为 ESP32-S3 开发板维护板级驱动包（BSP），重点在 `components/` 目录。
- 工作方式：允许重构架构，可调整原有 API，但要保持行为可解释、迁移路径清晰。
- 参考文档：`docs/bsp/README.md`

## 3) 硬件能力清单

- ST7789 LCD
- FT6336 触摸
- ES7210 / ES8311 音频
- QMI8658 IMU
- PCA9557 IO 扩展
- GC0308 摄像头
- ATGM336H GPS（GPIO10 <- GPS TX, GPIO11 -> GPS RX）

## 4) 构建与验证

- 进入 ESP-IDF 环境：`source ~/esp/esp-idf/export.sh`
- 项目构建命令：`idf.py build`
- 说明：不要使用 `idf.py -B build_ninja build`

## 5) 可参考资料

- https://wiki.lckfb.com/zh-hans/szpi-esp32s3/
- https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/introduction.html

## 6) 已确认约束（用户已给出）

- API 兼容策略：完全可破坏（允许直接调整/重命名原有 API，不保留兼容层）。
- 外设优先级：`display > touch > sd > imu > audio > camera > gps`。
- 验收标准：每个组件都要提供并可运行一个 `test_app`。
- GPS 约束：固定使用 `UART1`（`IO10 <- GPS_TX`，`IO11 -> GPS_RX`）。
- 自动化回归要求：仅要求仓库根目录 `idf.py build` 通过。
- 测试偏好：优先无人工自动化测试；无法自动验证的项目需明确标注“待人工确认”。
- 资源与性能约束：
  - 依赖 PSRAM
  - 启动速度无硬性要求
  - 性能优先
  - LCD 目标帧率：`30fps`
  - 触摸延迟目标：
    - 触摸采样延迟：`P95 <= 25ms`（峰值可放宽到 `40ms`）
    - 触摸到画面反馈：`P95 <= 66ms`（理想值 `<= 50ms`）
  - Audio 常用采样率：`16k`
  - Camera 性能：非关键项
- 硬件真值表文档：`docs/hw/boards/doers3_truth_table.md`。

## 7) 待补充信息（可后续逐步完善）

- 引脚与总线真值表：已整理在 `docs/hw/boards/doers3_truth_table.md`。
- 资源与性能约束（补充细项，TBD）
  - 目的：指导实现取舍（比如缓存大小、轮询频率、任务优先级）。
  - 待补充：RAM/PSRAM 预算数值、CPU 占用上限、功耗目标。
- 测试与回归（补充细项，TBD）
  - 已确认自动化门槛：仓库根目录 `idf.py build` 通过。
  - 待补充：是否需要额外硬件回归清单（若需要，再定义最小人工步骤）。

## 8) 建议默认值（若用户暂未指定）

当 7) 未补充时，Codex 可采用以下默认策略推进开发：

- 真值表：默认以 `docs/hw/boards/doers3_truth_table.md` 为基准；若发现与实现冲突，需先记录差异再改动。
- 资源与性能：在满足稳定性的前提下按“性能优先”取舍；避免明显浪费（超大静态缓冲、忙等循环）。
- 测试与回归：至少保证每个组件 `test_app` 可构建；核心优先级组件（display/touch/sd/imu）优先做真机验证。

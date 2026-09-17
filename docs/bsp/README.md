# 文档导航

仓库文档和代码入口的索引. `components/bsp` 是仓库唯一对外交付的 BSP 组件;仓库目的,构成和快速开始见根 `README.md`.

## 文档

| 文档 | 内容 |
|---|---|
| `docs/bsp/capabilities.md` | 能力承诺: 双板能力矩阵,public API 一览,不承诺边界 |
| `docs/bsp/status.md` | 进度: 已完成,未验证,暂停和下一步 |
| `docs/bsp_design.md` | BSP 结构,分层职责和 API 语义 |
| `docs/bsp/porting-guide.md` | 新增 board port 的接入指南 |
| `docs/hw/boards/doers3_truth_table.md` | DoerS3 硬件事实和验证记录 |
| `docs/hw/boards/auras3_truth_table.md` | AuraS3 硬件事实和验证记录 |
| `docs/hw/auras3-display-te.md` | AuraS3 TE 防撕裂实验记录 |
| `docs/hw/auras3-pmu-key.md` | AuraS3 PMU / KEY2 阶段性结论 |
| `components/shell/README.md` | 调试 shell 使用说明 |
| `AGENTS.md` | coding agent 协作规则 |

## 代码入口

```text
components/bsp/include/          # app 可见 BSP API
components/bsp/src/common/       # 跨板复用组合逻辑
components/bsp/src/boards/       # board port
components/bsp/src/drivers/      # BSP 私有 driver
components/bsp/test_app/         # BSP 能力验证工程
components/shell/                # 可选调试组件,BSP 不依赖它
```

## 验证入口

```sh
cd components/bsp/test_app
./bsp.sh <app> <auras3|aura|doers3|doer> build
./bsp.sh <app> <auras3|aura|doers3|doer> build flash monitor
```

当前 app: `audio`, `camera`, `pmu`, `shell`, `ui`.

`imu`, `sdcard`, `gnss` 测试已合并到 shell 命令;board info 由 shell `bsp info` 验证,不保留独立 test_app.

`ui` app 使用 `lv_demo_widgets()` 作为压力测试: UI task priority 6,stack 16384,LVGL log off.

`bsp.sh` 会在 app 目录生成真实 `sdkconfig` 和 `build/`,切换 board 时自动清理 app-local `sdkconfig` 和 `build/`.

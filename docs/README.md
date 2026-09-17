# 文档索引

本文件是仓库文档索引和文档分工的唯一出处,维护规则见 `AGENTS.md` §5. 仓库目的,构成和快速开始见根 `README.md`;coding agent 协作规则见 `AGENTS.md`.

## 文档

`<board>` 当前为 `doers3` 和 `auras3`.

| 文档 | 内容 |
|---|---|
| `bsp/design.md` | BSP 结构,分层职责和 API 语义 |
| `bsp/capabilities.md` | 能力承诺: 双板能力矩阵,public API 一览,不承诺边界 |
| `bsp/status.md` | 进度: 已完成,未验证,暂停和下一步 |
| `bsp/porting-guide.md` | 新增 board port 的接入指南 |
| `hw/boards/<board>/truth_table.md` | 板级硬件事实和验证记录 |
| `hw/boards/<board>/README.md` | 板级资料入口和官方来源 |
| `hw/specs/README.md` | 器件资料索引 (datasheet,原理图,未入库参考) |
| `hw/auras3-display-te.md` | AuraS3 TE 防撕裂实验记录 |
| `hw/auras3-pmu-key.md` | AuraS3 PMU / KEY2 阶段性结论 |
| `../components/shell/README.md` | 调试 shell 使用说明 |

## 文档分工

各文档的角色:

- truth table: 板级硬件事实 — pin,bus,芯片连接,待确认项和验证记录;板级事实的唯一出处.
- `bsp/capabilities.md`: 对外承诺的唯一出处 — 已承诺和明确不承诺的边界.
- `bsp/status.md`: 进度面 — 已完成,未验证,暂停和下一步;不重复承诺边界.
- `bsp/design.md`: BSP 结构,分层职责和 API 语义.
- `bsp/porting-guide.md`: board port 接入指南和每个文件的实现模板.
- 实验记录 (`hw/auras3-*.md`): 过程,取舍和理由;结论只写一次,不重复上面的承诺表.
- `../AGENTS.md`: 协作规则;架构禁项和 API 约束以 §3, §4 为准.

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

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
| `bsp/audio.md` | 音频子系统: 信号链,通道模型,用法和已知边界 |
| `hw/boards/<board>/truth_table.md` | 板级硬件事实和验证记录 |
| `hw/boards/<board>/README.md` | 板级资料入口和官方来源 |
| `code/<board>/README.md` | 板厂例程参考的提取范围和对应 BSP 能力 |
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
- `bsp/audio.md`: 音频的硬件连接, 通道模型和用法; 能力承诺以 `bsp/capabilities.md` 为准.
- 实验记录 (`hw/auras3-*.md`): 过程,取舍和理由;结论只写一次,不重复上面的承诺表.
- `code/<board>/`: 板厂例程参考,第三方代码,不适用本仓库的 API 约束和文档风格规则.
- `../AGENTS.md`: 协作规则;架构禁项和 API 约束以 §3, §4 为准.

## 代码入口

```text
components/bsp/include/          # app 可见 BSP API
components/bsp/src/common/       # 跨板复用组合逻辑
components/bsp/src/boards/       # board port
components/bsp/src/drivers/      # BSP 私有 driver
test/                            # 板级自检工程
components/shell/                # 可选调试组件,BSP 不依赖它
```

## 验证入口

自检工程的使用方式, 输出格式, `sdkconfig.defaults` 约定和 `sha` 语义见 `test/README.md`;每个模块的自检 app 在 `test/<module>/`, 统一入口是 `test/bsp.sh`.

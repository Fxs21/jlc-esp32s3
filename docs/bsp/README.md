# BSP 文档导航

`components/bsp` 是仓库唯一对外交付的 BSP 组件. 仓库目的,构成和快速开始见根 `README.md`;本文只做 BSP 文档和代码入口的导航.

## 文档入口

| 文档 | 内容 |
|---|---|
| `docs/bsp/capabilities.md` | 双板能力矩阵和 public API 一览 |
| `docs/bsp/status.md` | 当前进度,已验证项,暂停项和下一步 |
| `docs/bsp_design.md` | 结构,分层职责和 API 语义 |
| `docs/bsp/porting-guide.md` | 新增 board port 的接入指南 |
| `docs/hw/boards/doers3_truth_table.md` | DoerS3 硬件事实 |
| `docs/hw/boards/auras3_truth_table.md` | AuraS3 硬件事实 |

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

`bsp.sh` 会在 app 目录生成真实 `sdkconfig` 和 `build/`,切换 board 时自动清理 app-local `sdkconfig` 和 `build/`.

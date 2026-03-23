# BSP

BSP 现在收敛为一个公开组件：`components/bsp`。

目标不是描述每个文件，而是保证三件事清楚：

1. 要做什么
2. 已经做了什么
3. 还没做什么

## 当前方向

- 对外只有一组统一 BSP API，例如 `bsp_display.h`、`bsp_ui.h`、`bsp_sdcard.h`、`bsp_gnss.h`。
- 板级差异进入 `components/bsp/src/boards/<board>/`。
- DoerS3 与 AuraS3 的实现彼此分离，只通过相同公共接口对外呈现。
- Kconfig 只选择当前板子，不配置每个 GPIO 或外设参数。
- IOEXP 不再是公开 BSP 模块，只是 DoerS3 内部基础设施。
- 不引入 `bsp_ctrl_pin`，先保持直白实现。
- BSP test_app 放在 `components/bsp/test_app/<name>`，先保证可构建，再逐项真机确认。

## 文档入口

- 当前状态：`docs/bsp/status.md`
- 设计边界：`docs/bsp_design.md`
- DoerS3 硬件真值表：`docs/hw/boards/doers3_truth_table.md`

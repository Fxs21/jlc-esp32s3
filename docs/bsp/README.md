# BSP

`components/bsp` 是项目唯一公开 BSP 组件,用于为 DoerS3 和 AuraS3 提供统一,稳定,简单的板级 API.

app 应只依赖 `components/bsp/include` 中的公开头文件,不直接依赖 board port,私有 driver,GPIO 映射或芯片寄存器细节.

## 当前方向

- 一组公共 API,同时支持 DoerS3 和 AuraS3 board port.
- board 差异放在 `components/bsp/src/boards/<board>/`.
- 小 IC driver 优先由 BSP 私有管理,放在 `components/bsp/src/drivers/`.
- 跨板复用逻辑只放在 `components/bsp/src/common/`,例如 shared I2C bus owner,common UI lifecycle,LVGL buffer helper 和 unsupported stubs.
- 核心 API 不直接暴露 ESP-IDF 或第三方 driver 类型.
- LVGL 原生对象只通过明确命名的 escape hatch 暴露,例如 `bsp_ui_get_lvgl_display()`.
- `components/shell` 不是 BSP 的一部分,但 `components/bsp/test_app/shell` 可以把 shell 作为 BSP 调试入口.

## 目录入口

```text
components/bsp/include/          # app 可见 BSP API
components/bsp/src/common/       # 跨板复用组合逻辑
components/bsp/src/boards/       # board port
components/bsp/src/drivers/      # BSP 私有 driver
components/bsp/test_app/         # BSP 能力验证工程
components/shell/                # 可选调试组件,BSP 不依赖它
```

## 文档入口

- 设计边界: `docs/bsp_design.md`
- 当前状态: `docs/bsp/status.md`
- DoerS3 硬件真值表: `docs/hw/boards/doers3_truth_table.md`
- AuraS3 硬件真值表: `docs/hw/boards/auras3_truth_table.md`
- AuraS3 display TE 记录: `docs/hw/auras3-display-te.md`
- AuraS3 PMU / KEY2 记录: `docs/hw/auras3-pmu-key.md`
- Agent 协作规则: `AGENTS.md`

## 验证入口

BSP test_app 位于:

```text
components/bsp/test_app/<name>
```

当前 app:

```text
audio
board
camera
gnss
imu
pmu
sdcard
shell
touch
ui
```

常用方式:

```bash
cd components/bsp/test_app
./bsp.sh <app> <auras3|aura|doers3|doer> build
./bsp.sh <app> <auras3|aura|doers3|doer> build flash monitor
```

`bsp.sh` 会在 app 目录生成真实 `sdkconfig` 和 `build/`,并在切换 board 时自动清理 app-local `sdkconfig` / `build/`.

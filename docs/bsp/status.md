# BSP 当前状态

本文档只记录 `components/bsp` 的进度面: 已完成,未验证和下一步. 板级细节和证据在各板 truth table 中,能力承诺在 `docs/bsp/capabilities.md`,自检工程的使用方式和输出约定在 `test/README.md`.

- 全仓库文档,代码入口和验证入口索引: `docs/README.md`

## 验证规则 (2026-09-23 定稿)

- 硬件验收不再由 shell 承担;shell 只保留调试能力 (寄存器读写,信息查询),不作为任何模块的通过判据.
- 唯一判据来源是 `test/<module>/` 自检 app 打印的 `SELFTEST` 汇总行: 上电即自检,程序自己判 `PASS` / `FAIL` / `SKIP`.
- 验证基线 sha = 跑自检时的 HEAD. 之后的文档提交不使结论失效;驱动或公共 API 的代码提交会使该模块回到待复测.
- 例程删除判据: 例程里有价值的信息 (寄存器序列,时序,地址,取舍理由) 已固化进 driver 或 truth table,且该模块无"未验证"标记.
- 删除按逐模块闭环执行: 自检通过 -> 逐项核对例程 -> 差异写入 truth table §14 -> 删例程 -> 提交.

## 已完成

- `test/` 自检工程落地: 统一入口 `test/bsp.sh`,公共骨架 `test/selftest/`,约定见 `test/README.md`.
- AuraS3 前三个模块走完新式自检闭环,真机 `PASS` (2026-09-23, sha `99a464c`): `imu` 6/6,`sdcard` 8/8,`touch` 5/5;对应例程 `03_QMI8658`,`04_SD_MMC`,`07_Touch` 已核对删除,结论见 `docs/hw/boards/auras3/truth_table.md` §14.
- 旧口径真机能力 (未按新式自检复测): DoerS3 display,touch,backlight,sdcard,imu,audio,camera,gnss;AuraS3 display,backlight,audio,pmu;两板 shell 调试入口和 UI 正常.
- camera viewfinder (capture -> byte-swap -> display) 连续通路真机通过.
- 测量数据,日志和逐项细节见各板 truth table.

## 未验证 / 暂停

- AuraS3 除 `imu` / `sdcard` / `touch` 外的模块还没走新式自检: `test/` 下 `audio`,`camera`,`pmu`,`shell`,`ui` 仍是迁移前写法 (`TEST START` / `TEST PASS`),`display`,`backlight`,`gnss` 和诊断用 `i2c` 的 app 尚未建立.
- AuraS3 GNSS: 板上未贴模组 (原理图预留),只确认未接器件时 shell `gnss read` 返回超时.
- AuraS3 audio full-duplex (同时 playback + record) 未真机验证;`supports_full_duplex` 当前两板声明 true.
- AuraS3 `audio rec-rms` 的 MIC1/MIC2 RMS 未补测.
- AuraS3 触摸多点能力未真机确认,当前只承诺单点 (`max_points = 1`).
- QMI8658 `CTRL1` 的 `BE` 位与手册 Table 22 描述不一致,当前沿用板厂小端解析;需要绝对精度时复测.
- TE wait 默认不启用,后续研究参考 `docs/hw/auras3-display-te.md`.
- `docs/bsp/design.md` §6 的 Audio / PMU 两段复核暂停,待后续设计时一起处理,待改点见"下一步".

## 下一步

1. 阶段4 (AuraS3 铺开): 把 `display`,`backlight`,`ui`,`pmu`,`audio` 按自检骨架补判据并真机复测;`gnss` 硬件缺失,自检输出 `SKIP`;`camera` 不适用,输出 `SKIP`.
2. 阶段5 (AuraS3 收尾): 每完成一个模块就走一次删除闭环;剩余例程 `01_AXP2101`,`02_PCF85063`,`05_LVGL_WITH_RAM`,`06_I2SCodec`.
3. 阶段6 (DoerS3): 用同一套自检 app 复测全部模块.
4. `AGENTS.md` §6 与本节验证规则冲突的两处待改: "简单 I2C/UART 外设的验证合并到 shell 命令,不保留独立 test_app",以及 shell 承担硬件测试的表述.
5. PMU: 先做 internal-only software power-off 验证 (USB,仅电池,USB+电池三种场景和 `KEY2` 重新开机行为),再决定是否增加 public `shutdown` API.
6. 确认 AXP2101 rail 到 `VCC3V3` / `VCCRTC` / 外设电源的映射;验证前不开放 DCDC/LDO control.
7. AuraS3 GNSS 待硬件: 贴装模组后才能验证 `38400` baud,TX/RX 方向和 `GPS_RST` reset 极性.
8. 设计 `bsp_rtc` public API 前,先确认 `PCF85063` 的实际产品需求.
9. 复核 `docs/bsp/design.md` §6 的 Audio / PMU 两段. 已记录待改点: Audio 的 handle 共用措辞,desc 能力位说明,`S16_LE` / 16-bit / 8k-48k 约束;PMU 的 "只读" 措辞 (`open()` 实际会做 ADC / IRQ 最小使能),`bsp_pmu_config_t` 字段注释,`get_events()` 依赖 `enable_irq`.

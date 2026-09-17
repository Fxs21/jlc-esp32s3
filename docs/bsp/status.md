# BSP 当前状态

本文档只记录 `components/bsp` 的进度面: 已完成,未验证和下一步. 板级细节和证据在各板 truth table 中,能力承诺在 `docs/bsp/capabilities.md`.

- 能力承诺和 public API 边界: `docs/bsp/capabilities.md`
- 设计规则和 API 语义: `docs/bsp_design.md`
- 硬件事实: `docs/hw/boards/<board>_truth_table.md`

## 已完成

- DoerS3: display,touch,backlight,sdcard,imu,audio,camera,gnss 全部真机通过;shell 和 UI 正常.
- AuraS3: display,touch,backlight,sdcard,imu,audio,pmu 真机通过;shell 和 UI 正常.
- camera viewfinder (capture -> byte-swap -> display) 连续通路真机通过.
- 测量数据,日志和逐项细节见各板 truth table.

## 未验证 / 暂停

- AuraS3 GNSS 硬件未连接: 只确认未接模块时 shell `gnss read` 返回超时.
- AuraS3 audio full-duplex (同时 playback + record) 未真机验证;`supports_full_duplex` 当前两板声明 true.
- AuraS3 `audio rec-rms` 的 MIC1/MIC2 RMS 未补测.
- TE wait 默认不启用,后续研究参考 `docs/hw/auras3-display-te.md`.
- `docs/bsp_design.md` §6 的 Audio / PMU 两段复核暂停,待后续设计时一起处理,待改点见"下一步".

## 下一步

1. PMU: 先做 internal-only software power-off 验证 (USB,仅电池,USB+电池三种场景和 `KEY2` 重新开机行为),再决定是否增加 public `shutdown` API.
2. 确认 AXP2101 rail 到 `VCC3V3` / `VCCRTC` / 外设电源的映射;验证前不开放 DCDC/LDO control.
3. 等 AuraS3 GNSS 硬件连接后,验证 `38400` baud,TX/RX 方向和 `GPS_RST` reset 极性.
4. 设计 `bsp_rtc` public API 前,先确认 `PCF85063` 的实际产品需求.
5. 复核 `docs/bsp_design.md` §6 的 Audio / PMU 两段. 已记录待改点: Audio 的 handle 共用措辞,desc 能力位说明,`S16_LE` / 16-bit / 8k-48k 约束;PMU 的 "只读" 措辞 (`open()` 实际会做 ADC / IRQ 最小使能),`bsp_pmu_config_t` 字段注释,`get_events()` 依赖 `enable_irq`.

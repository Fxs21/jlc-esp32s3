# BSP 当前状态

本文档只记录 `components/bsp` 的进度面: 已完成,未验证和下一步. 板级细节和证据在各板 truth table 中,能力承诺在 `docs/bsp/capabilities.md`,自检工程的使用方式和输出约定在 `test/README.md`.

- 全仓库文档,代码入口和验证入口索引: `docs/README.md`

## 验证规则 (2026-09-23 定稿)

- 硬件验收不再由 shell 承担;shell 只保留调试能力 (寄存器读写,信息查询),不作为任何模块的通过判据.
- 唯一判据来源是 `test/<module>/` 自检 app 打印的 `SELFTEST` 汇总行: 上电即自检,程序自己判 `PASS` / `FAIL` / `SKIP`.
- 验证基线 sha = 跑自检时的 HEAD. 之后的文档提交不使结论失效;驱动或公共 API 的代码提交会使该模块回到待复测.
- 例程删除判据: 例程里有价值的信息 (寄存器序列,时序,地址,取舍理由,以及例程或驱动 README 里的硬件要求和注意事项) 已固化进 driver 或 truth table,且该模块无"未验证"标记.
- 删除按逐模块闭环执行: 自检通过 -> 逐项核对例程 -> 差异写入 truth table §14 -> 删例程 -> 提交.

## 已完成

- `test/` 自检工程落地: 统一入口 `test/bsp.sh`,公共骨架 `test/selftest/`,约定见 `test/README.md`.
- AuraS3 前三个模块走完新式自检闭环,真机 `PASS` (2026-09-23, sha `99a464c`): `imu` 6/6,`sdcard` 8/8,`touch` 5/5;对应例程 `03_QMI8658`,`04_SD_MMC`,`07_Touch` 已核对删除,结论见 `docs/hw/boards/auras3/truth_table.md` §14.
- AuraS3 `display` 走完新式自检,真机 `PASS` (2026-09-24, sha `57e98bd`): 7/7,含 3 个人工项;期间修复 AuraS3 panel 引用计数 (close 未归零) 让重开拿到空 panel 的缺陷;例程 `05_LVGL_WITH_RAM` 同时覆盖 `ui`.
- AuraS3 `backlight` 走完新式自检,真机 `PASS` (2026-09-24, sha `129b1ca`): 6/6,含 2 个人工项 (亮度分档, 0% -> 100% 恢复);期间修正 selftest 脏检查漏掉未跟踪文件的缺陷.
- AuraS3 `ui` 走完新式自检,真机 `PASS` (2026-09-30, sha `0430fbb`): 6/6,含 1 个人工项 (内置 widgets demo 渲染完整, 触摸跟手);期间把 ui 改成全程只 open 一次,规避面板复位亮闪;例程 `05_LVGL_WITH_RAM` 已对照删除,结论见 `docs/hw/boards/auras3/truth_table.md` §14.
- AuraS3 `shell` 走完新式自检,真机 `PASS` (2026-09-30, sha `5f4674c`): 4/4;自检结束后 repl 保留供人工调试;期间 `bsp info` 补回 `audio` / `pmu` / `camera` 三行 desc.
- AuraS3 `camera`,`gnss` 自检 app 落地,真机复跑按预期输出 `SKIP` (2026-09-30, sha `5f4674c`): camera 板上无, GNSS 模组未贴装.
- AuraS3 `rtc` 走完新式自检闭环,真机 `PASS` (2026-09-30, sha `f54f62a`): 5/5;期间新增 `bsp_rtc` public API 和 `test/rtc`,例程 `02_PCF85063` 已对照删除,结论见 `docs/hw/boards/auras3/truth_table.md` §14.
- AuraS3 `audio` 走完新式自检闭环,真机 `PASS` (2026-10-08, sha `5b2bc27`): 10/10,1 项人工项 pending (台位未接喇叭, 响度待补测);期间把 audio 重做成会话模型 public API (三种 record mask, TDM 回采, full-duplex),例程 `06_I2SCodec` 已对照删除,结论见 `docs/hw/boards/auras3/truth_table.md` §14.
- AuraS3 `pmu` 走完新式自检闭环,真机 `PASS` (2026-10-08, sha `90dc3f7-dirty`): 7 项 0 失败 1 ignored (未接电池, 插拔电池项 pending);期间修复 `open` 失败未清空 `handle_out`,以及"新 `open` 后 ADC 首轮转换未完成即断言"两处缺陷;软件关机 USB 场景真机验证: 断电成功, 无自动回电, 关机后短按 `KEY2` 可开机;例程 `01_AXP2101` 已对照删除,结论见 `docs/hw/boards/auras3/truth_table.md` §14.
- AuraS3 `i2c` 走完新式自检, 真机 `PASS` (2026-10-08, sha `acfcff3-dirty`): 4/4; 扫描到 7 个设备与 truth table I2C 清单一致; 该 app 兼作总线诊断入口, 完整扫描表随日志输出.
- DoerS3 `i2c` 走完新式自检, 真机 `PASS` (2026-10-08, sha `32e06a9`): 4/4, 干净 sha; 扫描到 5 个设备与 truth table 一致; 阶段 6 (DoerS3 复测) 首个模块.
- DoerS3 `imu` 走完新式自检闭环, AuraS3 `imu` 同 sha 复测 (2026-10-09, sha `e6ed4b6`): 双板各 6/6 PASS; 期间把 `CTRL1` 改为显式小端 (`0x60` -> `0x40`), 等待不进 driver, `open()` 契约明确为 "配置 + 独占 + 单位", 首样本由调用方用 `bsp_imu_is_data_ready()` 把关; 真机实证 `BE` 位不生效, 手册 Table 22 矛盾处按实测; 例程 `02-attitude` 已核对删除, 结论见 `docs/hw/boards/doers3/truth_table.md` §5.
- DoerS3 `sdcard` 复测 (2026-10-09, sha `5fecc25`): 8/8 PASS, 干净 sha; 卡 `SDABC` SDHC 30003.5 MB, 1-bit @ 20 MHz; 例程 `03-micro_sd` 已核对删除, 结论见 `docs/hw/boards/doers3/truth_table.md` §5.
- DoerS3 `display` 复测 (2026-10-09, sha `f416fd8`): 7/7 PASS, 3 个人工项 `yes`, 干净 sha;复测期间修复 `fill_rect` 异步缓冲复用竞争 (frame 图案底部残留白条);例程 `06-lcd` 已核对删除, 结论见 `docs/hw/boards/doers3/truth_table.md` §5.
- DoerS3 `backlight` 复测 (2026-10-09, sha `0f54ecd`): 6/6 PASS, 2 个人工项 `yes`, 干净 sha;顺带修复重复 open 时误报的 `W ledc: GPIO 42 is not usable` 警告;配置对照并入 §5 `06-lcd` 条目.
- DoerS3 `touch` 复测 (2026-10-10, sha `caeaa8a`): 5/5 PASS, 干净 sha;例程 `08-lcd_lvgl` 的 touch 部分已核对, ui 部分待 `bsp_ui` 复测后一并补记并删除.
- 旧口径真机能力 (未按新式自检复测): DoerS3 audio,camera,gnss;两板 shell 调试入口和 UI 正常.
- camera viewfinder (capture -> byte-swap -> display) 连续通路真机通过.
- 测量数据,日志和逐项细节见各板 truth table.

## 未验证 / 暂停

- AuraS3 `pmu` 电池相关项未验证: 无电池, 自检插拔电池项 `IGNORE`;软件关机只验证了 USB 场景, 仅电池与 USB+电池场景待电池到位.
- AuraS3 `pmu` VBUS_INSERT/REMOVE 事件未验证: USB 串口同时供电和出日志, 制造不了 VBUS 拔插; 需要电池供电 + 独立日志通道的台位.
- AuraS3 GNSS: 板上未贴模组 (原理图预留),贴装前 baud / TX,RX 方向 / reset 极性都无从验证,自检只能输出 `SKIP`.
- AuraS3 audio 喇叭响度未验证: 台位未接喇叭, 自检人工项 pending;回采判据与喇叭无关, 接上喇叭后补测该人工项.
- DoerS3 audio (含回采和 full-duplex) 未复测: board port 已对齐新设计, `supports_full_duplex` 暂为 `false`.
- AuraS3 触摸多点能力未真机确认,当前只承诺单点 (`max_points = 1`).
- QMI8658 gyro 量程换算挂起 (2026-10-09): 驱动 enum 把 `CTRL3` 的 `gFS=100` 记为 `512dps`, 手册 Table 22 为 ±256dps; 若按手册, gyro 读数偏大约 2 倍; 先不改代码, 修复后需双板复测 `imu`.
- TE wait 默认不启用,后续研究参考 `docs/hw/auras3-display-te.md`.
- AuraS3 RTC 掉电保持未验证: 板上无备份电池 (`VBACKUP` 接 `VBAT2`),拔电丢时是预期行为,装电池后补测.

## 下一步

1. 阶段4 (AuraS3 铺开): `pmu` 自检与 USB 场景软件关机,`i2c` 总线自检已完成;剩余 `gnss` 待模组贴装,当前自检输出 `SKIP`.
2. 阶段5 (AuraS3 收尾): 每完成一个模块就走一次删除闭环;AuraS3 例程 `01`~`07` 已全部核对删除.
3. 阶段6 (DoerS3): 进行中, 用同一套自检 app 复测全部模块; `i2c`,`imu`,`sdcard`,`display`,`backlight`,`touch` 已完成, 其余模块待测 (audio 暂跳过).
4. PMU: 仅电池 / USB+电池两种供电场景的软件关机行为待电池到位后验证 (USB 场景已完成, 记录见 `docs/hw/auras3-pmu-key.md`);验证入口是 `test/pmu` 自检结束后的引导步.
5. 确认 AXP2101 rail 到 `VCC3V3` / `VCCRTC` / 外设电源的映射;验证前不开放 DCDC/LDO control.
6. AuraS3 GNSS 待硬件: 贴装模组后才能验证 `38400` baud,TX/RX 方向和 `GPS_RST` reset 极性.

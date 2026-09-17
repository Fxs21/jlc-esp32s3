# AGENTS Instructions

本文件是给 Codex / coding agent 的协作规则,不是对外用户文档.

## 1. 项目目标

仓库目的,分层和正式承载应用的状态见根 `README.md` §1;进度,未验证项和暂停项以 `docs/bsp/status.md` 为准.

## 2. 工作方式

- 优先简单,直接,可解释的实现,不为了"未来可能需要"提前做复杂抽象.
- 公共 API 允许破坏性调整,不需要保留历史兼容层. 破坏性调整必须同步更新所有引用旧符号的位置,包括 `docs/bsp/`,`README.md`,`AGENTS.md`,test_app,shell 命令和 `tools/check.sh`;改动前用 `rg` 搜旧符号名逐个确认,退役符号加进 `tools/check.sh` 的旧符号表.
- 除非用户明确要求修改代码或文档,否则先分析问题,提出方案和建议,不主动改文件.
- 用户设定的长期项目规则应同步到 `AGENTS.md`;临时任务进度和一次性讨论不写入.
- 遇到不明确,存在明显取舍或可能影响后续架构的问题时,应主动询问用户,而不是自行拍板;如果已有更合适的最佳实践,应说明原因并给出建议,由用户决定是否采用.
- 不要回避已暴露的问题,也不要只给绕行建议;应正面定位根因,给出可执行修复路径,并在必要时修改实现.

## 3. API 约束

- 核心 BSP API 只暴露 `esp_err_t`,基础 C 类型,BSP 自有 `struct` / `enum`.
- 核心 BSP API 不直接暴露 ESP-IDF,LVGL 或第三方 driver 类型.
- 公共 API 不能被任何一块板的私有细节污染.
- 如确需暴露原生对象,只能作为明确命名的 integration / escape hatch. 当前只有两个: `bsp_ui_get_lvgl_display()` 暴露 LVGL 类型,`bsp_i2c_acquire()` 暴露原生 I2C bus handle;新增或删除例外必须同时更新本条和 `tools/check.sh` 的白名单.
- 公开头文件的 include 边界由 `tools/check.sh` 强制: 除上述例外,只允许 C 标准库头,其他 BSP 公开头和 `esp_err.h`.
- 外设生命周期优先使用 `open/close`;运行态操作使用清楚的动词,例如 `read/write/start/stop/capture/release`.
- 不做 runtime registry,不做通用 `bsp_hal_*`,不做大 `boarddb`,不做运行时 board detect.
- 不把 BSP 做成应用框架: 不提供 app 生命周期,事件循环或 task 管理.
- 不承诺暴露每个芯片的完整能力.
- 未经确认的硬件事实,slot layout,bus 时序或实验能力,不进入稳定公共 API.

## 4. Driver 与 Board Port 约束

- BSP 优先直接管理板上小芯片 driver,避免 app 或 board port 通过 ESP-IDF Component Manager 直接依赖零散外部 driver.
- 小芯片 driver 放在 `components/bsp/src/drivers/`,作为 BSP 私有实现,不进入公共 API.
- driver 应保持小而专,只负责芯片级寄存器读写和最小状态控制.
- board port 负责 pin,bus,address,power/reset sequence,默认电平,共享资源生命周期和板级语义.
- 可以参考已有开源/托管组件的寄存器序列和基础读写实现,但不要求为了"纯净"而完全对照手册从零实现.
- 如果多个 board 确实复用同一芯片 driver,可以在 `src/drivers/` 内复用;但不要为了假想复用提前设计通用 HAL.
- 除 camera,UI,display panel 这类大集成外,QMI8658,PCA9557,ES8311,ES7210 等小 IC driver 优先 BSP 私有化. display panel 复用 `esp_lcd` 生态现成驱动: DoerS3 用 IDF 内置 ST7789, AuraS3 用外部组件 `espressif/esp_lcd_co5300`.

## 5. 文档风格

- 文档正文可以写中文, 但标点统一使用英文半角标点.
- 代码路径, API 名称, 命令, 配置项和日志关键字使用 Markdown inline code 标记.

## 6. 验证原则

- 复杂能力 (多外设组合,需要人工观察) 应有独立 `components/bsp/test_app/<name>`,例如 `ui`,`camera`,`audio`,`pmu`.
- 简单 I2C/UART 外设 (如 IMU,GNSS) 和通用调试能力 (backlight,SD) 的验证合并到 shell 命令,不保留独立 test_app.
- test_app 只依赖公开 BSP API,不 include board port 或私有 driver 头文件.
- 自动化优先验证可构建,可运行的最小路径;需要人工动作的测试应在日志中明确提示.
- 无法自动验证的项必须标注并写明缺什么条件才能验证: 硬件事实类写入对应 truth table,未确认项标 `待真机确认` 或列入 `待裁决项`;验证进度类写入 `docs/bsp/status.md` 的 `## 下一步`.
- 调试日志可以临时增加,但必须服务于明确假设;得出结论并修复后,应删除或降级临时日志.
- 构建 ESP-IDF 工程前使用 `source ~/esp/esp-idf/export.sh`.
- 项目构建命令优先使用 `idf.py build`;test_app 的构建目录统一为 `build/`,不要自造其他目录名.
- 修改代码或文档后, 如果改动涉及 BSP, API, 文档或依赖, 应运行 `tools/check.sh`.
- 如果用户明确要求构建验证, 或明确允许 build, 再运行 `CHECK_BUILD=1 tools/check.sh`.
- 若 `tools/check.sh` 失败, 应先修复失败项, 不要跳过.

## 7. 当前阶段注意事项

- 当前优先保证 DoerS3 路径稳定可用;AuraS3 可以先保留 stub.
- DoerS3 硬件事实以 `docs/hw/boards/doers3_truth_table.md` 为准;若实现与真值表冲突,应先记录差异再修改.
- AuraS3 硬件事实以 `docs/hw/boards/auras3_truth_table.md` 为准;若实现与真值表冲突,应先记录差异再修改. truth table 中标注待确认的项,当前允许对应 board port 返回 `ESP_ERR_NOT_SUPPORTED` 或保留 stub.
- Audio 在两块板都只承诺 ES8311 speaker playback 和 ES7210 MIC1/MIC2 16-bit stereo record.
- MIC3 playback reference,TDM,AEC 当前暂停,不进入稳定 BSP API;full-duplex 未真机验证,验证前 app 不应依赖 `bsp_audio_desc_t.supports_full_duplex`.
- shell 不是 BSP 的一部分;`components/bsp` 不应依赖 shell.
- 修改 board port 代码后,应同步检查对应 truth table (`docs/hw/boards/<board>_truth_table.md`) 是否需要更新.例如 pin 分配变化,bus 类型切换,I2C 地址修正,新增或删除外设等都应反映到 truth table 中.

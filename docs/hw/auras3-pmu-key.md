# AuraS3 PMU 与按键说明

本文记录 AuraS3 电源管理相关原理图分析,并把已经确认的硬件事实,真机验证结果和仍需验证的事项分开.

## 已确认的原理图事实

- `AXP2101` 是 AuraS3 的 PMU,挂在主 I2C 总线上,地址为 `0x34`.
- `KEY1` 连接 `GPIO0` 到 GND,是 ESP32-S3 的 BOOT/download 按键,不是电源键.
- `KEY2` 连接 AXP2101 的 `PWRON` 输入到 GND,是 PMU 电源按键.
- `AXP_IRQ` 接到 `TCA9554` 的 `P5` / `EXIO5`,ESP32-S3 需要通过 I2C IO expander 读取.
- `SYS_OUT` 接到 `TCA9554` 的 `P4` / `EXIO4`,固件通过 I2C IO expander 读取它的原始逻辑电平.
- `GPS_RST` 接到 `TCA9554` 的 `P7` / `EXIO7`,当前 AuraS3 BSP 已经用于 GNSS reset 控制.
- `CHIP_PU` 是 ESP32-S3 的芯片使能/复位输入.高电平使能芯片,低电平让芯片保持复位/禁用.它不是用户电源键,也不是 AXP2101 的电源输出控制脚.

## CHIP_PU

`CHIP_PU` 是 ESP32-S3 自身的 enable/reset 脚,作用类似常见 ESP32 开发板上的 `EN`:

- `CHIP_PU = 1`:ESP32-S3 运行;
- `CHIP_PU = 0`:ESP32-S3 被复位或禁用;
- 上电时配合 RC 网络形成上电复位;
- 下载/调试电路也可能通过它复位芯片.

它只能影响 ESP32-S3 本体是否运行,不能直接关闭 AXP2101,也不能直接切断整板电源.因此不要把 `CHIP_PU` 当成产品电源键或 PMU shutdown 控制脚.

## KEY1

`KEY1` 连接到 `GPIO0`.它的作用是 ESP32-S3 启动模式选择:

- 松开:复位/上电时 `GPIO0` 为高,正常启动;
- 按住并复位/上电:`GPIO0` 被拉低,进入 ROM download 模式.

`KEY1` 不应参与产品电源管理.

## KEY2 / PWRON

`KEY2` 是原理图中唯一直接和电源管理相关的按键.按下时,它会把 AXP2101 的 `PWRON` 输入拉低.

预期作用:

- 在 ESP32-S3 固件运行前作为硬件开机键;
- 在固件运行后作为 PMU power-key 事件来源;
- 可以产生 AXP2101 `PKEY_SHORT` / `PKEY_LONG` IRQ;
- 长按可以触发 PMU 默认硬件关机;
- 关机后可以通过 `KEY2` 重新开机.

固件应优先读取 AXP2101 的 `PKEY_SHORT` / `PKEY_LONG` IRQ 状态,而不是把 `KEY2` 当成普通 ESP32 GPIO,因为 `KEY2` 并没有直接接到 ESP32-S3 GPIO.

### KEY2 时序语义 (AXP2101 `REG27H`, 手册 6.13.2.25)

| 动作 | 阈值 | 结果 |
|---|---|---|
| 运行中快速点按 | < `IRQLEVEL` (默认 1.5 s) | 只产生 `PKEY_SHORT` 事件, 不断电 |
| 运行中按住 | ≥ `IRQLEVEL` | 产生 `PKEY_LONG` 事件 |
| 运行中按住不放 | ≥ `OFFLEVEL` (默认 6 s, 可配 4/6/8/10 s;受 `REG22H[1]` EFUSE 开关控制) | PMU 硬件关机 |
| 关机状态下按住 | ≥ `ONLEVEL` (EFUSE, 128 ms~2 s) | 开机 |

真机对照: 自检引导项的快速短按只锁存事件、板子继续运行 (2026-10-08);长按导致设备断开在 bring-up 已观察到,与 `OFFLEVEL` 行为一致;软件关机 (`REG10H[0]`, `bsp_pmu_power_off()`) 与按键时长无关,由 `test/pmu` 自检提示处输入 `y` 触发.

## SYS_OUT / EXIO4

`SYS_OUT` 通过 `TCA9554` 的 `P4` / `EXIO4` 暴露给固件.从 KEY2/PWRON/NMOS 局部电路看,它更像与 KEY2/PWRON 状态相关的 readback/status 信号,而不是可控的系统电源 rail.

BSP bring-up 期间曾把它作为内部 raw 电平观察:

```c
// internal debug only: SYS_OUT / EXIO4 level
```

真机 PMU 循环观察确认:

```text
KEY2 松开/idle : SYS_OUT=0
KEY2 按下      : SYS_OUT=1
```

因此 `SYS_OUT` 可以作为 `KEY2` 当前按下状态的 internal raw readback 使用,但不进入 public PMU status,避免把 AuraS3 板级细节误抽象成所有板通用的 `power_button_pressed`.

## AXP_IRQ / EXIO5

`AXP_IRQ` 通过 `TCA9554` 的 `P5` / `EXIO5` 暴露给固件.这个线只表示 AXP2101 有 PMU 事件待处理,具体事件类型必须继续读取 AXP2101 IRQ status 寄存器.

对产品固件有用的事件包括:

- VBUS 插入/移除;
- 电池插入/移除;
- 充电开始/完成;
- power key 短按;
- power key 长按.

BSP bring-up 期间曾把它作为内部 raw 电平观察:

```c
// internal debug only: AXP_IRQ / EXIO5 level
```

真机 PMU 循环观察确认:

```text
idle    : AXP_IRQ=1
PMU IRQ : AXP_IRQ=0
```

因此 `AXP_IRQ` / `EXIO5` 是低有效中断线:空闲为高,AXP2101 有 pending IRQ 时拉低.固件仍必须读取 AXP2101 IRQ status 寄存器才能知道具体事件类型.


Bring-up 期间曾临时打印 raw IRQ status,并已经确认以下 AXP2101 IRQ 位和 BSP event 的关系:

| AXP2101 status | raw 位 | 含义 | BSP 映射 |
| --- | --- | --- | --- |
| `INTSTS1` / `0x48` | `0x10` | gauge new SOC / fuel gauge 更新 | 不映射 |
| `INTSTS2` / `0x49` | `0x01` | PWRON positive edge / KEY2 松开边沿 | 不映射 |
| `INTSTS2` / `0x49` | `0x02` | PWRON negative edge / KEY2 按下边沿 | 不映射 |
| `INTSTS2` / `0x49` | `0x04` | PWRON long press | `BSP_PMU_EVENT_POWER_KEY_LONG` |
| `INTSTS2` / `0x49` | `0x08` | PWRON short press | `BSP_PMU_EVENT_POWER_KEY_SHORT` |
| `INTSTS2` / `0x49` | `0x10` | battery remove | `BSP_PMU_EVENT_BATTERY_REMOVE` |
| `INTSTS2` / `0x49` | `0x20` | battery insert | `BSP_PMU_EVENT_BATTERY_INSERT` |
| `INTSTS3` / `0x4A` | `0x08` | charge start | `BSP_PMU_EVENT_CHARGE_START` |

Bring-up 期间出现过 `AXP_IRQ=0` 但 `bsp_pmu_get_events()` 返回 0 的情况:AXP2101 `REG40H` 复位默认把所有 IRQ 源打开,gauge SOC 更新或 KEY2 按下/松开边沿这类未映射事件也会拉低 `AXP_IRQ`.当前 driver 在 open 时把 IRQ 使能显式收敛到事件表映射的位 (`REG40H`=0,`REG41H`=0xFC,`REG42H`=0x18),预期此后 `AXP_IRQ` 只会随已映射事件拉低;该预期未被 `test/pmu` 自检覆盖,待后续专门验证.当前 public API 已删除 raw `INTSTS` 寄存器字段,只保留稳定的板级状态和已映射事件.

## 已完成的 KEY2 真机验证

使用 `test/pmu` 的 500ms 循环观察测试,已确认:

- KEY2 按下边沿会产生 AXP2101 `INTSTS2=0x02`,当前不映射为 BSP event;
- KEY2 松开边沿会产生 AXP2101 `INTSTS2=0x01`,当前不映射为 BSP event;
- `KEY2` 短按会产生 `BSP_PMU_EVENT_POWER_KEY_SHORT`,事件值为 `0x00000040`;
- `KEY2` 长按会产生 `BSP_PMU_EVENT_POWER_KEY_LONG`,事件值为 `0x00000080`;
- `KEY2` 按下期间 internal raw `SYS_OUT=1`,松开后回到 `0`;
- AXP2101 有 PKEY IRQ 待处理时 internal raw `AXP_IRQ=0`,事件清除后回到 `1`;
- 长按 `KEY2` 会导致设备断开,符合 AXP2101 默认硬件关机行为;
- 关机后短按 `KEY2` 可以重新开机 (2026-10-08 复测).

这说明 AuraS3 的 `KEY2 -> AXP2101 PWRON -> AXP_IRQ -> TCA9554 -> ESP32-S3` 事件链路已经跑通.


## 已完成的电池真机验证

插入电池时,PMU 循环观察已确认会产生电池插入和充电开始事件:

```text
columns     : idx sys_out axp_irq vbus bat chg standby  pct batmv     events
sample      : 024       0       0    1   1   1       0    0  4103 0x00000014
events      : 0x00000014
  battery   : insert=1 remove=0
  charge    : start=1 done=0
```

其中 `0x00000014` 表示:

- `BSP_PMU_EVENT_BATTERY_INSERT`,事件位 `0x00000004`;
- `BSP_PMU_EVENT_CHARGE_START`,事件位 `0x00000010`.

插着 USB 和电池启动时,已确认 AXP2101 可以读到电池存在,电池电压和充电状态:

```text
vbus        : present=1 good=1 voltage=5112mV
battery     : present=1 percent=0 voltage=4103mV
charge      : charging=1 discharging=0 standby=0 state=constant_current
system      : voltage=4323mV
```

后续真机测试确认,`battery_percent` 在刚插电池/刚上电后会正常收敛并工作;早期看到的 `percent=0` 属于阶段性观察结果,不再作为当前结论.AXP2101 的百分比来自 `BAT_PERCENT_DATA` / `0xA4`,它属于 fuel gauge 结果;手册和 XPowersLib 都显示还存在 `BAT_PARAME` / `0xA1`,`FUEL_GAUGE_CTRL` / `0xA2`,`CHARGE_GAUGE_WDT_CTRL` / `0x18` 等相关寄存器.当前第一阶段可以读取并展示 PMU 百分比,但产品 UI 仍建议做基本滤波,限速和异常值保护.

后续 200 次循环观察已确认拔掉电池会产生 `BSP_PMU_EVENT_BATTERY_REMOVE`,插回电池会产生 `BSP_PMU_EVENT_BATTERY_INSERT`,并且插回后通常伴随 `BSP_PMU_EVENT_CHARGE_START`:

```text
sample      : 044       0       0    1   0   0       1   -1    -1 0x00000018
events      : 0x00000018
  battery   : insert=0 remove=1
  charge    : start=1 done=0

sample      : 053       0       0    1   1   1       0   99  4200 0x00000014
events      : 0x00000014
  battery   : insert=1 remove=0
  charge    : start=1 done=0
```

其中 `0x00000018` 表示 battery remove + charge start,`0x00000014` 表示 battery insert + charge start.

## AXP2101 在 AuraS3 上相关的能力

AXP2101 不只是电池电压读取芯片,它还可以提供:

- VBUS 是否存在;
- VBUS 是否 good;
- 电池是否存在;
- 电池电压;
- 电池百分比;
- system voltage;
- PMU 内部温度;
- charging / discharging / standby 状态;
- 充电阶段,例如 trickle,precharge,constant current,constant voltage,done,not charging;
- VBUS,电池,充电,PWRON key 相关 IRQ 状态;
- 软件关机 (`REG10H[0]` Soft PWROFF);
- 可配置的 DCDC/LDO 电源 rail.

当前 BSP PMU 暴露状态,已映射事件和软件关机,不暴露 DCDC/LDO enable 或电压设置.原因是 AuraS3 上 rail 到外设的映射还没有完全验证,误改 rail 可能导致整板掉电或外设异常.

## AuraS3 第一阶段可以安全支持的范围

第一阶段 PMU 适合做:

- 检测并初始化 AXP2101;
- 启用电压/温度 ADC 通道;
- 关闭 TS ADC 通道;当前 BSP 不读取电池温度,且不修改 `REG50` 的 TS pin/充电温度保护配置;
- 读取 VBUS,电池,system voltage 和 PMU 温度;
- 读取充电状态和电池百分比;
- 通过 TCA9554 读取原始 `SYS_OUT` 和 `AXP_IRQ` 电平;
- 读取并清除 AXP2101 PMU 事件;
- 触发软件关机 (`REG10H[0]`);USB 场景已验证断电成功且不自恢复,关机后短按 `KEY2` 可开机.

这些足够支撑:

- 电池图标和充电图标;
- 充电开始/完成提示;
- 低电量提醒;
- PMU test_app 输出;
- KEY2 短按/长按事件验证.

## 软件关机 (真机验证)

`bsp_pmu_power_off()` 写 AXP2101 `REG10H[0]` (Soft PWROFF),预期切断 VCC3V3 等输出;断电后只能靠 PMU 开机源重新上电,当前板子上是 `KEY2` (PWRON).

验证方法: `cd test && ./bsp.sh pmu auras3 flash monitor`,自检汇总行之后按提示回答 `y` 触发;断电成功时本次运行不会再有任何输出,这属于预期. 按 `KEY2` 重新开机并重新跑自检,在提示处回答 `n` 跳过触发.

WSL + usbipd 环境下断电会让 USB 串口设备从 WSL 直接掉线,`idf.py monitor` 表现为 `device disconnected` 并等待重连;设备本身消失就是断电生效的直接证据,恢复观察前需要重新 attach.

| 场景 | 观察点 | 结论 |
|---|---|---|
| USB 供电 | 写 `REG10H[0]` 后是否断电;是否自动回电 | 断电成功 (2026-10-08): 最后日志停在 `>>> cutting power now`,USB 设备掉线;无自动回电;关机后短按 `KEY2` 可重新开机 |
| 仅电池 | 是否断电;`KEY2` 是否重新开机 | 待真机确认 (无电池) |
| USB + 电池 | 是否断电;是否自动回电;`KEY2` 行为 | 待真机确认 (无电池) |

## 暂不暴露的能力

以下能力在单独真机验证前,不应作为 public BSP API:

- DCDC/LDO rail enable/disable;
- DCDC/LDO voltage programming;
- 充电电流/目标电压的用户控制;
- 自动关机产品策略;
- power-key callback 分发.

这些能力要么属于后续经过板级验证的 PMU 扩展,要么属于应用层 power manager.

## 阶段性结论和后续事项

本阶段 PMU 测试到此收敛,第一版 BSP 可以确认支持:

- `KEY2` 短按 / 长按 PMU event;
- 长按 `KEY2` 硬关机,关机后按 `KEY2` 重新开机;
- `SYS_OUT` 作为 KEY2 raw 按下状态观察;
- `AXP_IRQ` 作为低有效 PMU IRQ 观察;
- VBUS,电池,system voltage,PMU 温度读取;
- 电池插入 / 拔出事件;
- 充电开始事件;
- 充电状态和 `battery_percent` 读取,且百分比在真机测试中可正常收敛.

当前 public BSP PMU API:`open` / `close` / `get_status` / `get_events` / `power_off`.Bring-up 期间使用过的 raw AXP2101 status / IRQ 寄存器已经证明有调试价值,但不进入稳定 public API.

后续若继续扩展 PMU,再单独验证:

1. AXP2101 哪些 DCDC/LDO rail 生成 `VCC3V3` 和 `VCCRTC`.
2. 是否还有其他 AXP2101 rail 接到外设,并且可以被安全控制.
3. 充电完成 IRQ 是否能在非满电电池充满时稳定触发.
4. VBUS 插入/移除 IRQ 是否正确映射.

`bsp_pmu_power_off()` 的断电行为已进入 public API;USB 场景已真机验证,仅电池与 USB+电池场景待电池到位,方法和记录见本文"软件关机 (真机验证)".

## 建议调试流程

1. 先运行 `test/pmu` 自检 (status 和事件部分).
2. 确认 AXP2101 chip ID 和基本电压读数.
3. 如需重新验证 KEY2/SYS_OUT 或 AXP_IRQ raw 电平,临时使用 board internal debug/test app,不要把 raw 字段加入 public PMU status.
4. 在 `KEY2` 短按和长按后读取并清除 PMU events.
5. software power-off 由 `test/pmu` 自检结束后的引导步触发;触发前确认 `KEY2` 可用作恢复手段.

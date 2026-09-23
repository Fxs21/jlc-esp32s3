# AuraS3 板级资料

Waveshare ESP32-S3-Touch-AMOLED-1.75.硬件真值 (pin,bus,地址,验证记录) 见 `truth_table.md`;器件手册见 `docs/hw/specs/`.

## 本目录

| 路径 | 内容 |
|---|---|
| `truth_table.md` | 硬件真值表: pin,bus,地址,待确认项和验证记录 |
| `schematic/schematic.pdf` | 官方原理图,3 页 (原文件 `ESP32-S3-Touch-AMOLED-1.75.pdf`) |
| `mechanical/auras3_3d_model.zip` | 结构件资料: DWG + 2D 图纸 + STP 3D 模型 (原 `ESP32-S3-Touch-AMOLED-1.75-3D.zip`) |
| `mechanical/auras3_3d_drawing_b.zip` | B 变体 2D 图纸 (DWG + PDF,无 3D 模型) |
| `docs/code/auras3/ESP-IDF-v5.4/01_AXP2101` ~ `07_Touch` | 从官方示例包提取的 ESP-IDF 例程,范围和约定见 `docs/code/auras3/README.md` |

## 官方示例代码包

上游示例包 (80MB,解压 294MB) 的内容:

| 目录 | 内容 |
|---|---|
| `Arduino-v3.1.0/examples/` | Arduino 示例,含 `03_LVGL_PCF85063_simpleTime`,`04_LVGL_QMI8658_ui`,`05_LVGL_AXP2101_ADC_Data` 等 |
| `Arduino-v3.1.0/libraries/` | Arduino 库,含板级 pin 定义 (`Mylibrary/pin_config.h`) |
| `ESP-IDF-v5.4/01_AXP2101` | PMU 示例 |
| `ESP-IDF-v5.4/02_PCF85063` | RTC 示例 |
| `ESP-IDF-v5.4/03_QMI8658` | IMU 示例 |
| `ESP-IDF-v5.4/04_SD_MMC` | SD 卡示例 |
| `ESP-IDF-v5.4/05_LVGL_WITH_RAM` | LVGL 示例 |
| `ESP-IDF-v5.4/06_I2SCodec` | ES8311/ES7210 音频示例 |
| `ESP-IDF-v5.4/07_Touch` | CST9217 触摸示例 |
| `ESP-IDF-v5.4/08_ESP32-S3-Touch-AMOLED-1_75-esp-brookesia` | esp-brookesia 综合示例 |
| `Firmware/` | 出厂固件 `ESP32-S3-Touch-AMOLED-1.75-FactoryOnly.bin` |

ESP-IDF `01` ~ `07` 已按"保留 `main/` 源码和资源,剔除构建产物"的规则提取到 `docs/code/auras3/ESP-IDF-v5.4/`.
例程内 vendored 的 `components/` 一并保留;Arduino 示例,`08_...esp-brookesia` 和 `Firmware/` 未提取.
原始 zip 已删除,需要时从官方来源重新下载.

## 官方来源

| 资料 | 链接 |
|---|---|
| 资料页 (wiki) | https://www.waveshare.net/wiki/ESP32-S3-Touch-AMOLED-1.75 |
| 商品页 | https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.75.htm |
| 示例代码仓库 | https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75 |

## 同硬件参考工程

工程清单见 `docs/hw/specs/README.md` 的"未入库参考".

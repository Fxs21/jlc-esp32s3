# 器件资料索引

板级和芯片级参考资料的入口. 规则:

- 原理图等板级资料放在 `docs/hw/boards/<board>/schematic/`;器件资料放在本目录.
- 目录: `esp32-s3/` 主控,`chips/` 器件,`panels/` 面板模组;面板模组的尺寸/FPC/光学/触控资料放 `panels/`,驱动 IC 手册仍放 `chips/`.
- 命名 `<part>_<doctype>[_<lang|rev>].pdf`: 全小写,无空格,无中文;doctype 取 `datasheet` / `trm` / `user_guide` / `porting_guide` / `app_note` / `schematic` / `panel`.
- truth table 和其它文档引用资料时写文件名,不写本地绝对路径.
- 整包 SDK,示例工程,厂商 wiki 不入库,登记在"未入库参考".

## 主控

| 器件 | 文件 | 厂商 | 版本/日期 |
|---|---|---|---|
| ESP32-S3 | `esp32-s3/esp32-s3_datasheet_cn.pdf` | Espressif | 版本 1.8 |
| ESP32-S3 | `esp32-s3/esp32-s3_datasheet_en.pdf` | Espressif | Version 1.6 |
| ESP32-S3 | `esp32-s3/esp32-s3_technical_reference_manual_cn.pdf` | Espressif | 版本 1.5,2024 |
| ESP32-S3 | `esp32-s3/esp32-s3_technical_reference_manual_en.pdf` | Espressif | Version 1.2,2023 |

## 板上器件

| 器件 | 文件 | 厂商 | 版本/日期 | 相关板 |
|---|---|---|---|---|
| QMI8658A | `chips/qmi8658a_datasheet_en.pdf` | QST | Rev A,2022-06-20 | 两板 |
| FT6336U | `chips/ft6336u_datasheet_v1.0.pdf` | FocalTech | V1.0 | DoerS3 |
| GC0308 | `chips/gc0308_datasheet.pdf` | GalaxyCore | 2010-01-28 | DoerS3 |
| ST7789V | `chips/st7789v_datasheet.pdf` | Sitronix | 版本见文档首页 | DoerS3 |
| ES8311 | `chips/es8311_datasheet.pdf` | Everest Semiconductor | 文档首页 | 两板 |
| ES8311 | `chips/es8311_user_guide_en.pdf` | Everest Semiconductor | 文档首页 | 两板 |
| ES7210 | `chips/es7210_datasheet.pdf` | Everest Semiconductor | 文档首页 | 两板 |
| ES7210 | `chips/es7210_user_guide_en.pdf` | Everest Semiconductor | Rev 1.00,2018-06-07 | 两板 |
| NS4150B | `chips/ns4150b_datasheet.pdf` | 纳芯威 (NSIWAY) | V1.1,2021-03 | 两板 |
| CO5300 | `chips/co5300_datasheet.pdf` | Chipone | V0.00,2023-03-28 | AuraS3 |
| CST9217 | `chips/cst9217_datasheet_v1.0.pdf` | 海栎创 (Hynitron) | V1.0 | AuraS3 |
| 海栎创触摸芯片 | `chips/hynitron_touch_driver_guide_v3.2.pdf` | 海栎创 (Hynitron) | v3.2,2021-04-27 | AuraS3 |
| AXP2101 | `chips/axp2101_datasheet_v1.0.pdf` | X-Powers | V1.0 (SWcharge) | AuraS3 |
| PCF85063A | `chips/pcf85063a_datasheet.pdf` | NXP | Rev. 7,2018-03-30 | AuraS3 |
| TCA9554 | `chips/tca9554_datasheet_cn.pdf` | TI | ZHCS616E,2017-02 | AuraS3 |
| PCA9557 | `chips/pca9557_datasheet.pdf` | NXP | Rev. 7,2013-12-10 | DoerS3 |
| MAX-M10S | `chips/max-m10s_datasheet.pdf` | u-blox | UBX-20035208 R06 | DoerS3 |

## 面板模组

| 模组 | 文件 | 厂商 | 规格 | 状态 |
|---|---|---|---|---|
| GDEY0154D67-T03 | `panels/gdey0154d67-t03_epaper_panel.pdf` | Dalian Good Display | 1.54 寸,200x200,1-bit 黑白,板载电容触摸 FT6336U,驱动 IC `ssd1681_datasheet.pdf`;文档日期 2023-08-09 | 候选外设,未接入任何板 |

## 原理图

| 板 | 文件 | 说明 |
|---|---|---|
| DoerS3 | `../boards/doers3/schematic/schematic.pdf` | 原文件 `SCH_ESP32-S3-V1_0_1_2026-04-20.pdf`,V1.0.1,3 页;另有分页图 `page-01..03.png` |
| AuraS3 | `../boards/auras3/schematic/schematic.pdf` | 原文件 `ESP32-S3-Touch-AMOLED-1.75.pdf`,Waveshare,3 页 |

## 未入库参考

| 资料 | 用途 | 状态 |
|---|---|---|
| Waveshare 官方 ESP-IDF BSP (`esp32_s3_touch_amoled_1_75`) | AuraS3 引脚和显示/触摸初始化的交叉参考 | 来源见 `../boards/auras3/README.md` |
| Arduino v3.3.5 `pin_config.h` (AuraS3) | AuraS3 引脚交叉对照 | 来源见 `../boards/auras3/README.md` |
| 立创实战派 ESP32-S3 官方例程包 | DoerS3 各外设的板厂参考实现 | 来源见 `../boards/doers3/README.md` |
| ESP32-S3 Hardware Design Guidelines | 硬件设计参考 | 未入库,URL 待补 |
| X-TRACK | LVGL 骑行码表参考工程;x-track 移植实验的上游 | 未入库;`https://github.com/FASTSHIFT/X-TRACK.git`,参考 commit `c42b8e5` |
| EasyGPS | GPS 终端参考工程结构 | 未入库;`https://github.com/ZhangKeLiang0627/EasyGPS.git`,参考 commit `337eaeb` |

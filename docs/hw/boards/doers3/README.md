# DoerS3 板级资料

立创实战派 ESP32-S3 (LCKFB SZPI-ESP32S3).硬件真值 (pin,bus,地址,验证记录) 见 `truth_table.md`;器件手册见 `docs/hw/specs/`.

## 本目录

| 路径 | 内容 |
|---|---|
| `truth_table.md` | 硬件真值表: pin,bus,地址,待确认项和验证记录 |
| `schematic/schematic.pdf` | 原理图,3 页 (原文件 `SCH_ESP32-S3-V1_0_1_2026-04-20.pdf`);分页图 `page-01..03.png` |
| `code/jlc_szp_doers3_examples.zip` | 官方例程包,未解压,待处理 (103MB,解压 340MB) |

## 官方来源

| 资料 | 链接 |
|---|---|
| 官方 wiki | https://wiki.lckfb.com/zh-hans/szpi-esp32s3/ |

## 官方例程包

`code/jlc_szp_doers3_examples.zip` 含 14 个例程:

| 例程 | 内容 |
|---|---|
| `01-boot_key` | BOOT 按键 |
| `02-attitude` | QMI8658 姿态 |
| `03-micro_sd` | SD 卡 |
| `04-audio_es7210` | ES7210 录音 |
| `05-audio_es8311` | ES8311 播放 |
| `06-lcd` | LCD |
| `07-lcd_camera` | LCD + GC0308 camera |
| `08-lcd_lvgl` | LCD + LVGL |
| `09-wifi_scan_connect` | WiFi 扫描/连接 (含 32MB 字体数组) |
| `10-ble_hid_device` | BLE HID |
| `11-mp3_player` | MP3 播放 |
| `12-speech_recognition` | 语音识别 |
| `13-human_face_detection` | 人脸检测 (含 vendored esp-dl,260MB) |
| `14-handheld` | 掌机 (含 35MB 素材) |

其中 `01` ~ `08` 是 BSP 的直接对照实现;整包因体积和 GitHub 100MB 单文件限制暂不入库,待提取源码子集.

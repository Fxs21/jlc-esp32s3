# DoerS3 板级资料

立创实战派 ESP32-S3 (LCKFB SZPI-ESP32S3).硬件真值 (pin,bus,地址,验证记录) 见 `truth_table.md`;器件手册见 `docs/hw/specs/`.

## 本目录

| 路径 | 内容 |
|---|---|
| `truth_table.md` | 硬件真值表: pin,bus,地址,待确认项和验证记录 |
| `schematic/schematic.pdf` | 原理图,3 页 (原文件 `SCH_ESP32-S3-V1_0_1_2026-04-20.pdf`);分页图 `page-01..03.png` |
| `docs/code/doers3/01-boot_key` ~ `08-lcd_lvgl` | 从官方例程包提取的 BSP 对照例程,范围和约定见 `docs/code/doers3/README.md` |

## 官方来源

| 资料 | 链接 |
|---|---|
| 官方 wiki | https://wiki.lckfb.com/zh-hans/szpi-esp32s3/ |

## 官方例程包

上游例程包 (103MB) 含 14 个例程:

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

其中 `01` ~ `08` 是 BSP 的直接对照实现,已按"保留源码和资源,剔除构建产物"的规则提取到 `docs/code/doers3/`.
`09` ~ `14` 是 WiFi,BLE,语音识别,人脸检测和应用综合示例,不属于板级 bring-up 参考,未提取.
原始 zip 已删除,需要时从官方来源重新下载.

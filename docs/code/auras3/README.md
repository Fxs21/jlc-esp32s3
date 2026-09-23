# AuraS3 例程参考

Waveshare ESP32-S3-Touch-AMOLED-1.75 官方示例包中的 ESP-IDF 例程 `01`~`07`,提取自 `waveshare_auras3_examples.zip`;Arduino 示例和整包未入库,来源见 `docs/hw/boards/auras3/README.md`.

| 例程 | 内容 | 对应 BSP 能力 |
|---|---|---|
| `01_AXP2101` | AXP2101 PMU | `bsp_pmu` |
| `02_PCF85063` | PCF85063A RTC | 未接入 BSP |
| `03_QMI8658` | QMI8658 IMU | `bsp_imu` |
| `04_SD_MMC` | SD 卡 | `bsp_sdcard` |
| `05_LVGL_WITH_RAM` | QSPI 面板 + LVGL | `bsp_ui` |
| `06_I2SCodec` | ES8311 / ES7210 音频 | `bsp_audio` |
| `07_Touch` | CST9217 触摸 | `bsp_touch` |

约定:

- 提取时保留 `main/` 源码和运行需要的资源 (`06_I2SCodec/main/canon.pcm`),不保留 `sdkconfig`,`sdkconfig.old` 和构建产物.
- 例程内 vendored 的第三方组件一并保留: `XPowersLib` (AXP2101),`SensorLib` (QMI8658,PCF85063,触摸),`esp_lcd_sh8601`.
- `SensorLib` 和 `XPowersLib` 是 MIT (lewis he);IDF 副本里没有 LICENSE,已从上游 Arduino 副本补入.
- `05_LVGL_WITH_RAM` 用的是 `esp_lcd_sh8601` 面板驱动;当前 BSP 的 AuraS3 display 走 `espressif/esp_lcd_co5300`,以 `docs/hw/boards/auras3/truth_table.md` 为准.
- 原始 zip 已删除;Arduino 示例和 `08_...esp-brookesia` 未保留,需要时从官方来源重新下载.

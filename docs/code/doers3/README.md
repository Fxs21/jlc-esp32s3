# DoerS3 例程参考

立创实战派 ESP32-S3 官方例程包中与 BSP 直接对照的 `01`~`08`,提取自 `jlc_szp_doers3_examples.zip`;整包 (`09`~`14`) 未入库,来源见 `docs/hw/boards/doers3/README.md`.

| 例程 | 内容 | 对应 BSP 能力 |
|---|---|---|
| `01-boot_key` | BOOT 按键 | 未进入 BSP API |
| `02-attitude` | QMI8658 姿态 | `bsp_imu` |
| `03-micro_sd` | SD 卡 | `bsp_sdcard` |
| `04-audio_es7210` | ES7210 录音 | `bsp_audio` |
| `05-audio_es8311` | ES8311 播放 | `bsp_audio` |
| `06-lcd` | ST7789 LCD | `bsp_display` |
| `07-lcd_camera` | LCD + GC0308 摄像头 | `bsp_camera` |
| `08-lcd_lvgl` | LCD + LVGL + 触摸 | `bsp_ui` |

约定:

- `main/esp32_s3_szp.c` 是板厂的板级封装,是 board port 的对照实现.
- 提取时保留例程源码和运行需要的资源 (`yingwu.h`,`logo_en_240x240_lcd.h`,`canon.pcm`),不保留 `sdkconfig`,`build/` 和 `managed_components/`.
- 例程依赖的 `espressif/es7210`,`espressif/es8311`,`espressif/esp32-camera`,`lvgl/lvgl` 等组件在各自 `main/idf_component.yml` 中声明,构建时由 Component Manager 拉取.
- 上游整包未附 LICENSE 文件;原始 zip 已删除,需要时从官方来源重新下载.

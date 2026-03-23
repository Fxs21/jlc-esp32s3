# bsp_ui

职责：把 BSP UI 设备接入 LVGL。

接口：

- `bsp_ui_open()`
- `bsp_ui_process()`
- `bsp_ui_get_display()`
- `bsp_ui_get_indev()`
- `bsp_ui_fill()`
- `bsp_ui_set_backlight()`
- `bsp_ui_close()`

说明：

- `bsp_ui` 是唯一 UI 入口
- `bsp_ui` 拥有 `display/touch/backlight` 生命周期
- `bsp_ui_open()` 默认不点亮屏幕
- 首帧黑屏或 reveal 策略由应用 / demo 显式决定

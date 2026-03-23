# bsp_camera

职责：管理 GC0308 生命周期和 framebuffer 获取/归还。

接口：

- `bsp_camera_present()`
- `bsp_camera_new()`
- `bsp_camera_start()`
- `bsp_camera_fb_get()`
- `bsp_camera_fb_return()`
- `bsp_camera_stop()`
- `bsp_camera_sensor()`
- `bsp_camera_del()`

说明：

- 通过 `bsp_camera_present()` 声明当前板是否有 camera
- PWDN 控制脚可来自直连 GPIO，也可来自 IO 扩展
- 当前仍维持单实例约束

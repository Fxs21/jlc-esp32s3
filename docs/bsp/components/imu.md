# bsp_imu

职责：管理 QMI8658 初始化和数据读取。

接口：

- `bsp_imu_present()`
- `bsp_imu_new()`
- `bsp_imu_del()`
- `bsp_imu_read()`
- `bsp_imu_is_data_ready()`

说明：

- 通过 `bsp_imu_present()` 声明当前板是否有 imu
- 后续如果板型变化，优先调整 backend 装配，不改上层 API

# bsp_i2c_bus

职责：管理共享 I2C bus 生命周期。

接口：

- `bsp_i2c_bus_default_cfg()`
- `bsp_i2c_bus_open()`
- `bsp_i2c_bus_close()`
- `bsp_i2c_bus_get()`
- `bsp_i2c_bus_is_ready()`

说明：

- 使用引用计数管理 bus
- 当前主要服务于 touch、audio、imu、camera、ioexp

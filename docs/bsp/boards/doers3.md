# DoerS3

## 状态

- 当前默认板型
- `bsp_boarddb_get()->name` 返回 `DoerS3`
- 硬件真值表：`docs/hw/boards/doers3_truth_table.md`

## 外设

- ST7789 LCD
- FT6336 触摸
- ES7210 / ES8311 音频
- QMI8658 IMU
- PCA9557 IO 扩展
- GC0308 摄像头
- GNSS / ATGM336H（UART1，`IO10 <- GPS_TX`，`IO11 -> GPS_RX`）

## 当前实现特点

- LCD CS、PA_EN、DVP_PWDN 通过 PCA9557 控制
- 共享 I2C 由 `bsp_i2c_bus` 管理
- IO 扩展由 `bsp_ioexp` 管理

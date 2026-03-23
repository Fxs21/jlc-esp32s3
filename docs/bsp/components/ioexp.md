# bsp_ioexp

职责：管理板级 IO 扩展器生命周期和输出状态。

接口：

- `bsp_ioexp_open()`
- `bsp_ioexp_close()`
- `bsp_ioexp_set_level()`

说明：

- 仅在板子确实使用 IO 扩展时才由上层组件申请
- 不要求所有板都带 PCA9557
- 当前 DoerS3 backend 是 PCA9557，寄存器操作位于私有 driver
- 是否通过 IO 扩展控制某个外设引脚，现由各组件 backend 的私有控制脚模型描述

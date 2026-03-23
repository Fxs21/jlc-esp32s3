# bsp_sdcard

职责：管理板载 SDMMC 1-bit 接口和文件系统挂载。

接口：

- `bsp_sdcard_get_desc()`
- `bsp_sdcard_open()`
- `bsp_sdcard_close()`
- `bsp_sdcard_mount()`
- `bsp_sdcard_unmount()`
- `bsp_sdcard_get_info()`
- `bsp_sdcard_get_fs_info()`

说明：

- SDMMC 接线、bus width、mount 参数来自 `bsp_boarddb`
- 当前不扩展为 storage 抽象，先保持 `bsp_sdcard`

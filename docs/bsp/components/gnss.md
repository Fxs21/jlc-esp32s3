# bsp_gnss

职责：管理 UART GNSS 设备的字节流读取。

接口：

- `bsp_gnss_get_desc()`
- `bsp_gnss_open()`
- `bsp_gnss_close()`
- `bsp_gnss_read()`

说明：

- 当前 DoerS3 使用 ATGM336H，固定 UART1
- UART 接线、波特率、buffer 和 timeout 来自 `bsp_boarddb`
- `bsp_gnss_read(handle, buf, len, out_len, timeout_ms)` 是唯一读取接口；`timeout_ms = 0` 表示非阻塞轮询
- 第一版只提供 raw bytes 读取，不内置 NMEA parser，也不在 BSP 内部做 line buffering

# bsp_audio

职责：统一播放、录音、音量和增益控制。

接口：

- `bsp_audio_get_desc()`
- `bsp_audio_default_stream_config()`
- `bsp_audio_new()`
- `bsp_audio_del()`
- `bsp_audio_start()`
- `bsp_audio_stop()`
- `bsp_audio_write()`
- `bsp_audio_read()`
- `bsp_audio_set_out_vol()`
- `bsp_audio_set_in_gain()`

说明：

- 通过 `bsp_audio_get_desc()` 声明当前板是否有 audio，以及是否具备 playback / record
- PA 使能脚可来自直连 GPIO，也可来自 IO 扩展
- 板差异只来自 `bsp_boarddb`
- 当前内部是 `bsp_audio -> es8311/es7210 driver` 两层，不再保留额外 board backend 选择层

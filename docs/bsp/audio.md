# 音频子系统

本文是 audio 的信号链, 通道模型和使用方式的出处. 硬件 pin, 地址和验证记录见 `docs/hw/boards/<board>/truth_table.md` 的 Audio 段; 能力承诺见 `docs/bsp/capabilities.md`; 进度和待补测项见 `docs/bsp/status.md`.

## 1. 信号链

两块板共用同一套拓扑: ESP32-S3 作 I2S master 产生 MCLK/BCLK/WS, ES8311 只作 DAC (播放), ES7210 作 ADC (录音).

```text
播放    ESP32-S3 DOUT -> ES8311 DSDIN -> ES8311 DAC -> OUTP/OUTN -> PA -> 喇叭
录音    ES7210 SDOUT/TDMOUT -> ESP32-S3 DIN
麦克风  MIC1, MIC2 -> ES7210 ADC
回采    ES8311 OUTP/OUTN -> ES7210 MIC3
```

- 播放和录音共用一组 MCLK/BCLK/WS: 一个会话只有一个时钟域和一种格式 (16-bit, 同一采样率), MCLK 固定 `256 x fs`.
- 回采点接在 PA 之前, 只验回采时不需要接喇叭.
- ES7210 `MIC3` 不是第三个板载麦克风; 它是 ES8311 模拟输出的回采, 用作 AEC reference.
- PA 由 board port 控制: `play_start()` 打开, `play_stop()` 关闭.

双板差异 (pin 全表在各板 truth table):

| 项目 | AuraS3 | DoerS3 |
|---|---|---|
| I2C 地址 | ES8311 `0x18` / ES7210 `0x40` | ES8311 `0x18` / ES7210 `0x41` |
| PA 控制 | `GPIO46` 直连 NS4150B | PCA9557 `IO1` |
| 回采 | ES8311 `OUTP/OUTN` -> ES7210 `MIC3P/MIC3N` | 同左 (经 `R34/R35=0R`) |
| 音频复测状态 | full-duplex 和回采真机 `PASS` | 未按新式自检复测 |

## 2. 通道模型

录音流按 `record_channel_mask` 交织, 一个 bit 一个通道, 位序就是 ES7210 TDM slot 顺序:

| mask | 录制通道 | 传输方式 |
|---|---|---|
| `0` | 无 (纯播放) | I2S TX mono |
| `MIC1 \| MIC2` | `[MIC1, MIC2]` | I2S STD, 16-bit stereo |
| `MIC1 \| MIC2 \| LOOPBACK` | `[MIC1, 回采, MIC2]` | TDM 4 slot, BSP 只暴露前 3 个 |

- 帧大小 = 通道数 `x 2` 字节; `record_read()` 的 `len` 必须是帧大小的整数倍.
- 回采在第二个通道 (slot 1) 是本板真机实测的 ES7210 TDM 顺序 (播放 1 kHz 时该通道有信号, 另两路安静).
- 除这三种组合外的 mask 返回 `ESP_ERR_NOT_SUPPORTED`.
- 采样率只接受 `8000`,`11025`,`12000`,`16000`,`22050`,`24000`,`32000`,`44100`,`48000` Hz.
- 增益: 板载麦默认 `30 dB`, 步进 3 dB, 上限 `37.5 dB`; 回采是 line 级信号, 推荐 `0 dB` (30 dB 会削顶).

## 3. 使用

一次 `bsp_audio_open()` = 一个会话; 播放和录音是会话里的两条独立流, 可以同时运行 (full-duplex).

```c
bsp_audio_config_t cfg = bsp_audio_default_config();  // 16 kHz, MIC1|MIC2
cfg.record_channel_mask = BSP_AUDIO_RECORD_CH_MIC1 | BSP_AUDIO_RECORD_CH_MIC2;  // 要回采再加 LOOPBACK
bsp_audio_handle_t audio;
ESP_ERROR_CHECK(bsp_audio_open(&cfg, &audio));

// 播放: 单声道 16-bit 帧
bsp_audio_play_set_volume(audio, 80);                 // 0..100
bsp_audio_play_start(audio);
bsp_audio_play_write(audio, pcm, bytes, &written, 100);
bsp_audio_play_stop(audio);

// 录音: 按 mask 位序交织的帧
bsp_audio_record_set_gain(audio, cfg.record_channel_mask, 30.0f);
bsp_audio_record_start(audio);
bsp_audio_record_read(audio, buf, sizeof(buf), &got, 100);
bsp_audio_record_stop(audio);

bsp_audio_close(audio);
```

- full-duplex: 一个 task 驱动 `play_*`, 另一个 task 驱动 `record_*`; 同一个方向的调用由调用者串行化.
- `play_write()` / `record_read()` 阻塞到 `timeout_ms` 并返回实际字节数.
- `record_start()` 在没有播放时也会保持 TX 时钟: 录音需要 master clock, 同时保证 TX 不会送残留数据.
- mask 为 `0` 的会话里所有 `record_*` 返回 `ESP_ERR_INVALID_STATE`; `record_set_gain()` 的 `channel_mask` 必须是会话 mask 的子集, 否则返回 `ESP_ERR_INVALID_ARG`.
- `play_start()` / `play_stop()` / `record_start()` / `record_stop()` 幂等; 二次 `open()` 返回 `ESP_ERR_INVALID_STATE`.

## 4. 已知边界

- 不提供 AEC 算法, 只提供回采通道作为参考信号.
- AuraS3 喇叭响度未验证 (台位未接喇叭); 回采判据与喇叭无关.
- DoerS3 代码已对齐同一设计, 但音频未按新式自检复测, `supports_full_duplex` 暂为 `false`.

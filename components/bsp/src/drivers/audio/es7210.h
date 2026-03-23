#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct es7210_s *es7210_handle_t;

#define ES7210_MIC1 (1u << 0)
#define ES7210_MIC2 (1u << 1)
#define ES7210_MIC3 (1u << 2)
#define ES7210_MIC4 (1u << 3)

typedef enum {
    ES7210_MCLK_FROM_PAD,
    ES7210_MCLK_FROM_CLOCK_DOUBLER,
} es7210_mclk_src_t;

typedef struct {
    i2c_master_bus_handle_t bus;
    uint8_t addr_7bit;
    uint8_t mic_mask;
    uint16_t mclk_div;
    bool master_mode;
    es7210_mclk_src_t mclk_src;
} es7210_config_t;

esp_err_t es7210_open(const es7210_config_t *cfg, es7210_handle_t *out_handle);
esp_err_t es7210_close(es7210_handle_t handle);
esp_err_t es7210_set_format(es7210_handle_t handle, uint32_t sample_rate, uint8_t bits_per_sample);
esp_err_t es7210_enable(es7210_handle_t handle, bool enable);
esp_err_t es7210_mute(es7210_handle_t handle, bool mute);
esp_err_t es7210_set_mic_mask(es7210_handle_t handle, uint8_t mic_mask);
esp_err_t es7210_set_gain(es7210_handle_t handle, float gain_db);
esp_err_t es7210_set_channel_gain(es7210_handle_t handle, uint8_t channel_mask, float gain_db);
esp_err_t es7210_read_register(es7210_handle_t handle, uint8_t reg, uint8_t *value);
esp_err_t es7210_write_register(es7210_handle_t handle, uint8_t reg, uint8_t value);
void es7210_dump_registers(es7210_handle_t handle);

#ifdef __cplusplus
}
#endif

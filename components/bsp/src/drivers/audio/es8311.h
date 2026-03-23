#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct es8311_s *es8311_handle_t;

#define ES8311_MODE_ADC (1u << 0)
#define ES8311_MODE_DAC (1u << 1)

typedef struct {
    i2c_master_bus_handle_t bus;
    uint8_t addr_7bit;
    uint8_t mode;
    uint16_t mclk_div;
    bool master_mode;
    bool use_mclk;
    bool digital_mic;
    bool invert_mclk;
    bool invert_sclk;
    bool no_dac_ref;
} es8311_config_t;

esp_err_t es8311_open(const es8311_config_t *cfg, es8311_handle_t *out_handle);
esp_err_t es8311_close(es8311_handle_t handle);
esp_err_t es8311_set_format(es8311_handle_t handle, uint32_t sample_rate, uint8_t bits_per_sample);
esp_err_t es8311_enable(es8311_handle_t handle, bool enable);
esp_err_t es8311_mute(es8311_handle_t handle, bool mute);
esp_err_t es8311_set_volume(es8311_handle_t handle, int volume);
esp_err_t es8311_set_mic_gain(es8311_handle_t handle, float gain_db);
esp_err_t es8311_read_register(es8311_handle_t handle, uint8_t reg, uint8_t *value);
esp_err_t es8311_write_register(es8311_handle_t handle, uint8_t reg, uint8_t value);
void es8311_dump_registers(es8311_handle_t handle);

#ifdef __cplusplus
}
#endif

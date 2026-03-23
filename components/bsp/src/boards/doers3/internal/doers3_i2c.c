#include "doers3_i2c.h"

#include "bsp_i2c_bus.h"
#include "doers3_pins.h"

static const bsp_i2c_bus_config_t s_i2c_config = {
    .tag = "doers3_i2c",
    .port = DOERS3_I2C_PORT,
    .sda = DOERS3_I2C_SDA,
    .scl = DOERS3_I2C_SCL,
    .glitch_ignore_cnt = DOERS3_I2C_GLITCH_IGNORE_CNT,
    .internal_pullup = DOERS3_I2C_INTERNAL_PULLUP,
};

static bsp_i2c_bus_handle_t s_i2c;

esp_err_t doers3_i2c_acquire(void)
{
    return bsp_i2c_bus_acquire(&s_i2c, &s_i2c_config);
}

esp_err_t doers3_i2c_release(void)
{
    return bsp_i2c_bus_release(&s_i2c);
}

esp_err_t doers3_i2c_get(i2c_master_bus_handle_t *out_bus)
{
    return bsp_i2c_bus_get(s_i2c, out_bus);
}

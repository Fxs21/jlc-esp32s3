#include "bsp_board.h"
#include "bsp_i2c.h"

#include "doers3_pins.h"

static const bsp_board_info_t s_info = {
    .id = BSP_BOARD_ID_DOERS3,
    .name = "DoerS3",
};

const bsp_board_info_t *bsp_board_get_info(void)
{
    return &s_info;
}

const bsp_i2c_bus_config_t *bsp_i2c_get_config(void)
{
    static const bsp_i2c_bus_config_t cfg = {
        .port = DOERS3_I2C_PORT,
        .sda = DOERS3_I2C_SDA,
        .scl = DOERS3_I2C_SCL,
        .glitch_ignore_cnt = DOERS3_I2C_GLITCH_IGNORE_CNT,
        .internal_pullup = DOERS3_I2C_INTERNAL_PULLUP,
    };
    return &cfg;
}

#include "bsp_board.h"
#include "bsp_i2c_internal.h"

#include "auras3_pins.h"

static const bsp_board_info_t s_info = {
    .id = BSP_BOARD_ID_AURAS3,
    .name = "AuraS3",
};

const bsp_board_info_t *bsp_board_get_info(void)
{
    return &s_info;
}

const bsp_i2c_bus_config_t *bsp_i2c_get_config(void)
{
    static const bsp_i2c_bus_config_t cfg = {
        .port = AURAS3_I2C_PORT,
        .sda = AURAS3_I2C_SDA,
        .scl = AURAS3_I2C_SCL,
        .glitch_ignore_cnt = AURAS3_I2C_GLITCH_IGNORE_CNT,
        .internal_pullup = AURAS3_I2C_INTERNAL_PULLUP,
    };
    return &cfg;
}

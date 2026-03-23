#include "sdcard.h"

#include "doers3_pins.h"

static const bsp_sdcard_pins_t s_pins = {
    .clk = DOERS3_SDCARD_CLK,
    .cmd = DOERS3_SDCARD_CMD,
    .d0  = DOERS3_SDCARD_D0,
};

const bsp_sdcard_pins_t *bsp_sdcard_get_pins(void)
{
    return &s_pins;
}

#include "bsp_board.h"

static const bsp_board_info_t s_info = {
    .id = BSP_BOARD_ID_AURAS3,
    .name = "AuraS3",
    .capabilities = 0,
};

const bsp_board_info_t *bsp_board_get_info(void)
{
    return &s_info;
}

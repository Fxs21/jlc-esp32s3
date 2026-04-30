#include "bsp_board.h"

static const bsp_board_info_t s_info = {
    .id = BSP_BOARD_ID_DOERS3,
    .name = "DoerS3",
    .capabilities = BSP_BOARD_CAP_DISPLAY |
                    BSP_BOARD_CAP_TOUCH |
                    BSP_BOARD_CAP_BACKLIGHT |
                    BSP_BOARD_CAP_SDCARD |
                    BSP_BOARD_CAP_GNSS |
                    BSP_BOARD_CAP_IMU |
                    BSP_BOARD_CAP_AUDIO,
};

const bsp_board_info_t *bsp_board_get_info(void)
{
    return &s_info;
}

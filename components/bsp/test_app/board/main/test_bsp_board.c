#include <inttypes.h>
#include <stdio.h>

#include "bsp_audio.h"
#include "bsp_backlight.h"
#include "bsp_board.h"
#include "bsp_camera.h"
#include "bsp_display.h"
#include "bsp_gnss.h"
#include "bsp_imu.h"
#include "bsp_sdcard.h"
#include "bsp_touch.h"

static void print_bool(const char *name, bool value)
{
    printf("%-12s: %s\n", name, value ? "yes" : "no");
}

void app_main(void)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    printf("board       : %s (%d)\n", board->name, board->id);
    printf("capabilities: 0x%" PRIx64 "\n", board->capabilities);

    const bsp_display_desc_t *display = bsp_display_get_desc();
    printf("display     : present=%d size=%ux%u backlight=%d touch=%d\n",
           display->present,
           display->width,
           display->height,
           display->has_backlight,
           display->has_touch);
    print_bool("touch", bsp_touch_get_desc()->present);
    print_bool("backlight", bsp_backlight_get_desc()->present);
    print_bool("sdcard", bsp_sdcard_get_desc()->present);
    print_bool("gnss", bsp_gnss_get_desc()->present);
    print_bool("imu", bsp_imu_get_desc()->present);
    print_bool("audio", bsp_audio_get_desc()->present);
    print_bool("camera", bsp_camera_get_desc()->present);
}

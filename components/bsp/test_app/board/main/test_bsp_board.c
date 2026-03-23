#include <inttypes.h>
#include <stdio.h>

#include "bsp_audio.h"
#include "bsp_backlight.h"
#include "bsp_board.h"
#include "bsp_camera.h"
#include "bsp_display.h"
#include "bsp_gnss.h"
#include "bsp_imu.h"
#include "bsp_pmu.h"
#include "bsp_sdcard.h"
#include "bsp_touch.h"
#include "esp_log.h"

#define TAG "test_bsp_board"

static void print_bool(const char *name, bool value)
{
    printf("%-12s: %s\n", name, value ? "yes" : "no");
}

void app_main(void)
{
    ESP_LOGI(TAG, "TEST START name=board");
    const bsp_board_info_t *board = bsp_board_get_info();
    printf("board       : %s (%d)\n", board->name, board->id);
    printf("capabilities: 0x%" PRIx64 "\n", board->capabilities);

    bsp_display_handle_t display = NULL;
    esp_err_t display_ret = bsp_display_open(&display);
    if (display_ret == ESP_OK) {
        const bsp_display_info_t *info = bsp_display_get_info(display);
        printf("display     : present=%d size=%ux%u bpp=%u align=%ux%u\n",
               info != NULL && info->present,
               info == NULL ? 0 : info->width,
               info == NULL ? 0 : info->height,
               info == NULL ? 0 : info->bits_per_pixel,
               info == NULL ? 0 : info->align_x,
               info == NULL ? 0 : info->align_y);
        ESP_ERROR_CHECK(bsp_display_close(display));
    } else {
        printf("display     : open failed: %s\n", esp_err_to_name(display_ret));
    }

    print_bool("touch", bsp_touch_get_desc()->present);
    print_bool("backlight", bsp_backlight_get_desc()->present);
    print_bool("sdcard", bsp_sdcard_get_desc()->present);
    print_bool("gnss", bsp_gnss_get_desc()->present);
    print_bool("imu", bsp_imu_get_desc()->present);
    print_bool("audio", bsp_audio_get_desc()->present);
    print_bool("pmu", bsp_pmu_get_desc()->present);
    print_bool("camera", bsp_camera_get_desc()->present);
    ESP_LOGI(TAG, "TEST PASS name=board summary=\"descriptors printed\"");
}

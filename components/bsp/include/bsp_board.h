#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_BOARD_ID_DOERS3 = 0,
    BSP_BOARD_ID_AURAS3,
} bsp_board_id_t;

#define BSP_BOARD_CAP_DISPLAY   (1ULL << 0)
#define BSP_BOARD_CAP_TOUCH     (1ULL << 1)
#define BSP_BOARD_CAP_BACKLIGHT (1ULL << 2)
#define BSP_BOARD_CAP_SDCARD    (1ULL << 3)
#define BSP_BOARD_CAP_GNSS      (1ULL << 4)
#define BSP_BOARD_CAP_IMU       (1ULL << 5)
#define BSP_BOARD_CAP_AUDIO     (1ULL << 6)
#define BSP_BOARD_CAP_CAMERA    (1ULL << 7)
#define BSP_BOARD_CAP_POWER     (1ULL << 8)

typedef struct {
    bsp_board_id_t id;
    const char *name;
    uint64_t capabilities;
} bsp_board_info_t;

const bsp_board_info_t *bsp_board_get_info(void);

#ifdef __cplusplus
}
#endif

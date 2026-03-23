#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_BOARD_ID_DOERS3 = 0,
    BSP_BOARD_ID_AURAS3,
} bsp_board_id_t;

typedef struct {
    bsp_board_id_t id;
    const char *name;
} bsp_board_info_t;

const bsp_board_info_t *bsp_board_get_info(void);

#ifdef __cplusplus
}
#endif

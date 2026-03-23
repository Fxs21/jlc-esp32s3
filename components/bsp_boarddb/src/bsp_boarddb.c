#include "bsp_boarddb.h"

#include "bsp_boarddb_priv.h"

const bsp_boarddb_desc_t *bsp_boarddb_get(void)
{
#if CONFIG_BSP_BOARD_AURAS3
    return &bsp_boarddb_auras3_desc;
#else
    return &bsp_boarddb_doers3_desc;
#endif
}

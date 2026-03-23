#include "bsp_board.h"
#include "bsp_i2c.h"

#include "doers3_i2c.h"

static const bsp_board_info_t s_info = {
    .id = BSP_BOARD_ID_DOERS3,
    .name = "DoerS3",
};

const bsp_board_info_t *bsp_board_get_info(void)
{
    return &s_info;
}

esp_err_t bsp_i2c_acquire_default(i2c_master_bus_handle_t *out_bus)
{
    if (out_bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_bus = NULL;

    esp_err_t ret = doers3_i2c_acquire();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = doers3_i2c_get(out_bus);
    if (ret != ESP_OK) {
        (void)doers3_i2c_release();
    }
    return ret;
}

esp_err_t bsp_i2c_release_default(void)
{
    return doers3_i2c_release();
}

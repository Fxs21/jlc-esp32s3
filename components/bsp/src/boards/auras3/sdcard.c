#include "bsp_sdcard.h"

#include <string.h>

#include "esp_check.h"

#define TAG "bsp_sdcard"

static const bsp_sdcard_desc_t s_desc = {
    .present = false,
};

const bsp_sdcard_desc_t *bsp_sdcard_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_sdcard_open(bsp_sdcard_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_sdcard_close(bsp_sdcard_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

esp_err_t bsp_sdcard_mount(bsp_sdcard_handle_t handle, const char *mount_point)
{
    (void)mount_point;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_sdcard_unmount(bsp_sdcard_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_ERR_NOT_SUPPORTED;
}

const char *bsp_sdcard_get_mount_point(bsp_sdcard_handle_t handle)
{
    (void)handle;
    return NULL;
}

const sdmmc_card_t *bsp_sdcard_card(bsp_sdcard_handle_t handle)
{
    (void)handle;
    return NULL;
}

esp_err_t bsp_sdcard_get_info(bsp_sdcard_handle_t handle, bsp_sdcard_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_info != NULL, ESP_ERR_INVALID_ARG, TAG, "out_info is null");
    memset(out_info, 0, sizeof(*out_info));
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_sdcard_get_fs_info(bsp_sdcard_handle_t handle, bsp_sdcard_fs_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_info != NULL, ESP_ERR_INVALID_ARG, TAG, "out_info is null");
    memset(out_info, 0, sizeof(*out_info));
    return ESP_ERR_NOT_SUPPORTED;
}

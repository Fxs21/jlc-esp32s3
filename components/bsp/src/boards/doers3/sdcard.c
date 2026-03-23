#include "bsp_sdcard.h"

#include <stdlib.h>
#include <string.h>

#include "doers3_pins.h"
#include "driver/sdmmc_default_configs.h"
#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "sd_protocol_defs.h"

#define TAG "bsp_sdcard"
#define ALLOC_UNIT_SIZE (32 * 1024)

struct bsp_sdcard_s {
    sdmmc_card_t *card;
    char mount_point[32];
};

static const bsp_sdcard_desc_t s_desc = {
    .present = true,
};

const bsp_sdcard_desc_t *bsp_sdcard_get_desc(void)
{
    return &s_desc;
}

static bsp_sdcard_type_t get_type(const sdmmc_card_t *card)
{
    if (card == NULL) {
        return BSP_SDCARD_TYPE_UNKNOWN;
    }
    if (card->is_sdio) {
        return BSP_SDCARD_TYPE_SDIO;
    }
    if (card->is_mmc) {
        return BSP_SDCARD_TYPE_MMC;
    }
    if ((card->ocr & SD_OCR_SDHC_CAP) == 0) {
        return BSP_SDCARD_TYPE_SDSC;
    }
    uint64_t capacity_bytes = (uint64_t)card->csd.capacity * (uint64_t)card->csd.sector_size;
    return capacity_bytes < (32ULL * 1024ULL * 1024ULL * 1024ULL) ? BSP_SDCARD_TYPE_SDHC : BSP_SDCARD_TYPE_SDXC;
}

esp_err_t bsp_sdcard_open(bsp_sdcard_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    struct bsp_sdcard_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_sdcard_close(bsp_sdcard_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = ESP_OK;
    if (handle->card != NULL) {
        first_err = bsp_sdcard_unmount(handle);
    }
    free(handle);
    return first_err;
}

esp_err_t bsp_sdcard_mount(bsp_sdcard_handle_t handle, const char *mount_point)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(mount_point != NULL, ESP_ERR_INVALID_ARG, TAG, "mount_point is null");
    ESP_RETURN_ON_FALSE(handle->card == NULL, ESP_ERR_INVALID_STATE, TAG, "already mounted");
    size_t mount_len = strnlen(mount_point, sizeof(handle->mount_point));
    ESP_RETURN_ON_FALSE(mount_len > 0 && mount_len < sizeof(handle->mount_point), ESP_ERR_INVALID_ARG, TAG, "bad mount point");
    ESP_RETURN_ON_FALSE(mount_point[0] == '/', ESP_ERR_INVALID_ARG, TAG, "mount_point must start with '/'");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_cfg.clk = DOERS3_SDCARD_CLK;
    slot_cfg.cmd = DOERS3_SDCARD_CMD;
    slot_cfg.d0 = DOERS3_SDCARD_D0;
    slot_cfg.d1 = GPIO_NUM_NC;
    slot_cfg.d2 = GPIO_NUM_NC;
    slot_cfg.d3 = GPIO_NUM_NC;
    slot_cfg.d4 = GPIO_NUM_NC;
    slot_cfg.d5 = GPIO_NUM_NC;
    slot_cfg.d6 = GPIO_NUM_NC;
    slot_cfg.d7 = GPIO_NUM_NC;
    slot_cfg.cd = SDMMC_SLOT_NO_CD;
    slot_cfg.wp = SDMMC_SLOT_NO_WP;
    slot_cfg.width = 1;
    slot_cfg.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mnt_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = ALLOC_UNIT_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_cfg, &mnt_cfg, &handle->card);
    if (ret != ESP_OK) {
        return ret;
    }
    memcpy(handle->mount_point, mount_point, mount_len + 1);
    return ESP_OK;
}

esp_err_t bsp_sdcard_unmount(bsp_sdcard_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->card != NULL, ESP_ERR_INVALID_STATE, TAG, "not mounted");
    ESP_RETURN_ON_ERROR(esp_vfs_fat_sdcard_unmount(handle->mount_point, handle->card), TAG, "unmount failed");
    handle->card = NULL;
    handle->mount_point[0] = '\0';
    return ESP_OK;
}

const char *bsp_sdcard_get_mount_point(bsp_sdcard_handle_t handle)
{
    return (handle == NULL || handle->mount_point[0] == '\0') ? NULL : handle->mount_point;
}

const sdmmc_card_t *bsp_sdcard_card(bsp_sdcard_handle_t handle)
{
    return handle == NULL ? NULL : handle->card;
}

esp_err_t bsp_sdcard_get_info(bsp_sdcard_handle_t handle, bsp_sdcard_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_info != NULL, ESP_ERR_INVALID_ARG, TAG, "out_info is null");
    memset(out_info, 0, sizeof(*out_info));
    if (handle->card == NULL) {
        return ESP_OK;
    }
    const sdmmc_card_t *card = handle->card;
    out_info->mounted = true;
    out_info->type = get_type(card);
    out_info->capacity_bytes = (uint64_t)card->csd.capacity * (uint64_t)card->csd.sector_size;
    out_info->sector_count = (uint64_t)card->csd.capacity;
    out_info->sector_size = (uint32_t)card->csd.sector_size;
    out_info->bus_width = 1;
    out_info->target_freq_khz = SDMMC_FREQ_DEFAULT;
    out_info->real_freq_khz = card->real_freq_khz;
    memcpy(out_info->name, card->cid.name, sizeof(card->cid.name));
    out_info->name[sizeof(card->cid.name)] = '\0';
    return ESP_OK;
}

esp_err_t bsp_sdcard_get_fs_info(bsp_sdcard_handle_t handle, bsp_sdcard_fs_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_info != NULL, ESP_ERR_INVALID_ARG, TAG, "out_info is null");
    memset(out_info, 0, sizeof(*out_info));
    if (handle->card == NULL || handle->mount_point[0] == '\0') {
        return ESP_OK;
    }
    out_info->mounted = true;
    return esp_vfs_fat_info(handle->mount_point, &out_info->total_bytes, &out_info->free_bytes);
}

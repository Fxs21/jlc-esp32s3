#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_boarddb.h"
#include "bsp_sdcard.h"
#include "driver/sdmmc_default_configs.h"
#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "sd_protocol_defs.h"

#define BSP_SDCARD_TAG "bsp_sdcard"
#define BSP_SDCARD_ALLOC_UNIT_SIZE (32 * 1024)

struct bsp_sdcard_s {
    sdmmc_card_t *card;
    char mount_point[32];
};

static bsp_sdcard_desc_t sdcard_desc;

static const bsp_boarddb_sdcard_desc_t *sdcard_cfg(void)
{
    return &bsp_boarddb_get()->sdcard;
}

const bsp_sdcard_desc_t *bsp_sdcard_get_desc(void)
{
    const bsp_boarddb_sdcard_desc_t *cfg = sdcard_cfg();
    sdcard_desc.present = cfg->present && cfg->kind == BSP_BOARDDB_SDCARD_KIND_SDMMC;
    return &sdcard_desc;
}

static bsp_sdcard_type_t bsp_sdcard_get_type(const sdmmc_card_t *card)
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
    if (capacity_bytes < (32ULL * 1024ULL * 1024ULL * 1024ULL)) {
        return BSP_SDCARD_TYPE_SDHC;
    }
    return BSP_SDCARD_TYPE_SDXC;
}

esp_err_t bsp_sdcard_open(bsp_sdcard_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(bsp_sdcard_get_desc()->present, ESP_ERR_NOT_SUPPORTED, BSP_SDCARD_TAG, "sdcard not present");

    struct bsp_sdcard_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_SDCARD_TAG, "no memory");

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_sdcard_close(bsp_sdcard_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "handle is null");
    esp_err_t first_err = ESP_OK;

    if (handle->card != NULL) {
        esp_err_t ret = bsp_sdcard_unmount(handle);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }

    free(handle);
    return first_err;
}

esp_err_t bsp_sdcard_mount(bsp_sdcard_handle_t handle, const char *mount_point)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(mount_point != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "mount_point is null");
    ESP_RETURN_ON_FALSE(handle->card == NULL, ESP_ERR_INVALID_STATE, BSP_SDCARD_TAG, "already mounted");
    size_t mount_len = strnlen(mount_point, sizeof(handle->mount_point));
    ESP_RETURN_ON_FALSE(mount_len > 0 && mount_len < sizeof(handle->mount_point),
                        ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "mount_point is too long");
    ESP_RETURN_ON_FALSE(mount_point[0] == '/', ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "mount_point must start with '/'");

    const bsp_boarddb_sdcard_desc_t *cfg = sdcard_cfg();
    ESP_RETURN_ON_FALSE(cfg->present && cfg->kind == BSP_BOARDDB_SDCARD_KIND_SDMMC,
                        ESP_ERR_NOT_SUPPORTED, BSP_SDCARD_TAG, "sdcard not present");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    if (cfg->max_freq_khz > 0) {
        host.max_freq_khz = cfg->max_freq_khz;
    }

    sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_cfg.clk = cfg->clk;
    slot_cfg.cmd = cfg->cmd;
    slot_cfg.d0 = cfg->d0;
    slot_cfg.d1 = cfg->d1;
    slot_cfg.d2 = cfg->d2;
    slot_cfg.d3 = cfg->d3;
    slot_cfg.d4 = GPIO_NUM_NC;
    slot_cfg.d5 = GPIO_NUM_NC;
    slot_cfg.d6 = GPIO_NUM_NC;
    slot_cfg.d7 = GPIO_NUM_NC;
    slot_cfg.cd = cfg->cd;
    slot_cfg.wp = cfg->wp;
    slot_cfg.width = cfg->width;
    slot_cfg.flags = cfg->slot_flags;

    esp_vfs_fat_sdmmc_mount_config_t mnt_cfg = {
        .format_if_mount_failed = cfg->format_if_mount_failed,
        .max_files = cfg->max_files,
        .allocation_unit_size = BSP_SDCARD_ALLOC_UNIT_SIZE,
        .disk_status_check_enable = cfg->disk_status_check_enable,
        .use_one_fat = cfg->use_one_fat,
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
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->card != NULL, ESP_ERR_INVALID_STATE, BSP_SDCARD_TAG, "not mounted");

    ESP_RETURN_ON_ERROR(esp_vfs_fat_sdcard_unmount(handle->mount_point, handle->card), BSP_SDCARD_TAG, "unmount failed");

    handle->card = NULL;
    handle->mount_point[0] = '\0';
    return ESP_OK;
}

const char *bsp_sdcard_get_mount_point(bsp_sdcard_handle_t handle)
{
    if (handle == NULL || handle->mount_point[0] == '\0') {
        return NULL;
    }
    return handle->mount_point;
}

const sdmmc_card_t *bsp_sdcard_card(bsp_sdcard_handle_t handle)
{
    if (handle == NULL) {
        return NULL;
    }
    return handle->card;
}

esp_err_t bsp_sdcard_get_info(bsp_sdcard_handle_t handle, bsp_sdcard_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_info != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "out_info is null");

    memset(out_info, 0, sizeof(*out_info));
    if (handle->card == NULL) {
        return ESP_OK;
    }

    const sdmmc_card_t *card = handle->card;
    const bsp_boarddb_sdcard_desc_t *cfg = sdcard_cfg();
    out_info->mounted = true;
    out_info->type = bsp_sdcard_get_type(card);
    out_info->capacity_bytes = (uint64_t)card->csd.capacity * (uint64_t)card->csd.sector_size;
    out_info->sector_count = (uint64_t)card->csd.capacity;
    out_info->sector_size = (uint32_t)card->csd.sector_size;
    out_info->bus_width = cfg->width;
    out_info->target_freq_khz = cfg->max_freq_khz;
    out_info->real_freq_khz = card->real_freq_khz;
    memcpy(out_info->name, card->cid.name, sizeof(card->cid.name));
    out_info->name[sizeof(card->cid.name)] = '\0';
    return ESP_OK;
}

esp_err_t bsp_sdcard_get_fs_info(bsp_sdcard_handle_t handle, bsp_sdcard_fs_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_info != NULL, ESP_ERR_INVALID_ARG, BSP_SDCARD_TAG, "out_info is null");

    memset(out_info, 0, sizeof(*out_info));
    if (handle->card == NULL || handle->mount_point[0] == '\0') {
        return ESP_OK;
    }

    out_info->mounted = true;
    return esp_vfs_fat_info(handle->mount_point, &out_info->total_bytes, &out_info->free_bytes);
}

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_sdcard_s *bsp_sdcard_handle_t;

typedef struct {
    bool present;
} bsp_sdcard_desc_t;

typedef enum {
    BSP_SDCARD_TYPE_UNKNOWN = 0,
    BSP_SDCARD_TYPE_SDSC,
    BSP_SDCARD_TYPE_SDHC,
    BSP_SDCARD_TYPE_SDXC,
    BSP_SDCARD_TYPE_MMC,
    BSP_SDCARD_TYPE_SDIO,
} bsp_sdcard_type_t;

typedef struct {
    bool mounted;
    bsp_sdcard_type_t type;
    uint64_t capacity_bytes;
    uint64_t sector_count;
    uint32_t sector_size;
    uint8_t bus_width;
    uint32_t target_freq_khz;
    int real_freq_khz;
    char name[9];
} bsp_sdcard_info_t;

typedef struct {
    bool mounted;
    uint64_t total_bytes;
    uint64_t free_bytes;
} bsp_sdcard_fs_info_t;

const bsp_sdcard_desc_t *bsp_sdcard_get_desc(void);

esp_err_t bsp_sdcard_open(bsp_sdcard_handle_t *out_handle);
esp_err_t bsp_sdcard_close(bsp_sdcard_handle_t handle);

esp_err_t bsp_sdcard_mount(bsp_sdcard_handle_t handle, const char *mount_point);
esp_err_t bsp_sdcard_unmount(bsp_sdcard_handle_t handle);
const char *bsp_sdcard_get_mount_point(bsp_sdcard_handle_t handle);

const sdmmc_card_t *bsp_sdcard_card(bsp_sdcard_handle_t handle);
esp_err_t bsp_sdcard_get_info(bsp_sdcard_handle_t handle, bsp_sdcard_info_t *out_info);
esp_err_t bsp_sdcard_get_fs_info(bsp_sdcard_handle_t handle, bsp_sdcard_fs_info_t *out_info);

#ifdef __cplusplus
}
#endif

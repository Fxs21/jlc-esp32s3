#include <inttypes.h>
#include <stdio.h>

#include "bsp_sdcard.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "test_bsp_sdcard"
#define TEST_MOUNT_POINT "/sdcard"

static const char *type_name(bsp_sdcard_type_t type)
{
    switch (type) {
    case BSP_SDCARD_TYPE_SDSC:
        return "SDSC";
    case BSP_SDCARD_TYPE_SDHC:
        return "SDHC";
    case BSP_SDCARD_TYPE_SDXC:
        return "SDXC";
    case BSP_SDCARD_TYPE_MMC:
        return "MMC";
    case BSP_SDCARD_TYPE_SDIO:
        return "SDIO";
    default:
        return "UNKNOWN";
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "TEST START name=sdcard");
    if (!bsp_sdcard_get_desc()->present) {
        ESP_LOGW(TAG, "TEST SKIP name=sdcard reason=\"not present\"");
        return;
    }

    bsp_sdcard_handle_t sd = NULL;
    esp_err_t ret = bsp_sdcard_open(&sd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=sdcard step=open err=%s", esp_err_to_name(ret));
        return;
    }

    ret = bsp_sdcard_mount(sd, TEST_MOUNT_POINT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=sdcard step=mount err=%s", esp_err_to_name(ret));
        (void)bsp_sdcard_close(sd);
        return;
    }

    bsp_sdcard_info_t info = {0};
    ret = bsp_sdcard_get_info(sd, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=sdcard step=get_info err=%s", esp_err_to_name(ret));
        goto out;
    }
    printf("mounted       : %d\n", info.mounted);
    const char *mount_point = NULL;
    ret = bsp_sdcard_get_mount_point(sd, &mount_point);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=sdcard step=get_mount_point err=%s", esp_err_to_name(ret));
        goto out;
    }
    printf("mount point   : %s\n", mount_point);
    printf("card name     : %s\n", info.name);
    printf("card type     : %s\n", type_name(info.type));
    printf("capacity bytes: %" PRIu64 "\n", info.capacity_bytes);
    printf("sector count  : %" PRIu64 "\n", info.sector_count);
    printf("sector size   : %" PRIu32 "\n", info.sector_size);
    printf("bus width     : %u\n", info.bus_width);
    printf("real freq kHz : %d\n", info.real_freq_khz);

    bsp_sdcard_fs_info_t fs = {0};
    ret = bsp_sdcard_get_fs_info(sd, &fs);
    if (ret == ESP_OK) {
        printf("fs total bytes: %" PRIu64 "\n", fs.total_bytes);
        printf("fs free bytes : %" PRIu64 "\n", fs.free_bytes);
    } else {
        ESP_LOGW(TAG, "get fs info failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "TEST PASS name=sdcard summary=\"mounted %s capacity=%" PRIu64 "\"", mount_point, info.capacity_bytes);

out:
    ret = bsp_sdcard_close(sd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST FAIL name=sdcard step=close err=%s", esp_err_to_name(ret));
    }
}

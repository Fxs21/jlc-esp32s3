#include "HAL.h"

#include "bsp_sdcard.h"
#include "esp_err.h"
#include "esp_log.h"

namespace {

struct SdState {
    bsp_sdcard_handle_t handle = nullptr;
    HAL::SD_CallbackFunction_t event_cb = nullptr;
    bool ready = false;
};

SdState sSd;
constexpr char TAG[] = "HAL_SD_CARD";

static const char *SdTypeName(bsp_sdcard_type_t type)
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
    case BSP_SDCARD_TYPE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

static bool SdReadInfo(bsp_sdcard_info_t *out_info)
{
    if (out_info == nullptr || sSd.handle == nullptr) {
        return false;
    }

    esp_err_t err = bsp_sdcard_get_info(sSd.handle, out_info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "bsp_sdcard_get_info failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

}  // namespace

bool HAL::SD_Init()
{
    if (sSd.handle == nullptr) {
        esp_err_t err = bsp_sdcard_open(&sSd.handle);
        if (err != ESP_OK) {
            sSd.ready = false;
            ESP_LOGE(TAG, "bsp_sdcard_open failed: %s", esp_err_to_name(err));
            return false;
        }
    }

    if (bsp_sdcard_card(sSd.handle) != nullptr) {
        sSd.ready = true;
        return true;
    }

    esp_err_t err = bsp_sdcard_mount(sSd.handle, "/sdcard");
    if (err != ESP_OK) {
        sSd.ready = false;
        ESP_LOGW(TAG, "bsp_sdcard_mount failed: %s", esp_err_to_name(err));
        return false;
    }

    sSd.ready = true;
    return true;
}

bool HAL::SD_GetReady()
{
    if (sSd.handle == nullptr) {
        return false;
    }

    bsp_sdcard_info_t info = {};
    if (!SdReadInfo(&info)) {
        sSd.ready = false;
        return false;
    }

    sSd.ready = info.mounted;
    return sSd.ready;
}

float HAL::SD_GetCardSizeMB()
{
    bsp_sdcard_info_t info = {};
    if (!SdReadInfo(&info) || !info.mounted) {
        return 0.0f;
    }

    return (float)((double)info.capacity_bytes / (1024.0 * 1024.0));
}

const char *HAL::SD_GetTypeName()
{
    bsp_sdcard_info_t info = {};
    if (!SdReadInfo(&info) || !info.mounted) {
        return "NONE";
    }

    return SdTypeName(info.type);
}

void HAL::SD_SetEventCallback(SD_CallbackFunction_t callback)
{
    sSd.event_cb = callback;
}

void HAL::SD_Update()
{
    // Current BSP API does not expose a card-detect GPIO yet.
}

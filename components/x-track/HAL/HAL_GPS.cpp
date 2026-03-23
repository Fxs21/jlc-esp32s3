#include "HAL.h"

#include <string.h>

#include "TinyGPSPlus/TinyGPS++.h"
#include "bsp_gnss.h"
#include "esp_err.h"
#include "esp_log.h"

namespace {

struct GpsState {
    TinyGPSPlus parser;
    bsp_gnss_handle_t handle = nullptr;
    bool ready = false;
};

GpsState sGps;
constexpr size_t kGpsRxChunkSize = 128;
constexpr char TAG[] = "HAL_GPS";
constexpr bool kGpsTransparentLogEnable = false;

}  // namespace

void HAL::GPS_Init()
{
    if (sGps.ready) {
        return;
    }

    const bsp_gnss_desc_t *desc = bsp_gnss_get_desc();
    if (desc == nullptr || !desc->present) {
        ESP_LOGW(TAG, "GNSS is not present on this board");
        return;
    }

    esp_err_t err = bsp_gnss_open(&sGps.handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_gnss_open failed: %s", esp_err_to_name(err));
        return;
    }

    sGps.ready = true;
    ESP_LOGI(TAG, "GNSS initialized");
}

void HAL::GPS_Update()
{
    if (!sGps.ready || sGps.handle == nullptr) {
        return;
    }

    uint8_t rx_buf[kGpsRxChunkSize];
    while (true) {
        size_t len = 0;
        esp_err_t err = bsp_gnss_read(sGps.handle, rx_buf, sizeof(rx_buf), &len, 0);
        if (err != ESP_OK || len == 0) {
            break;
        }

        if (kGpsTransparentLogEnable) {
            ESP_LOGI(TAG, "GPS RAW: %.*s", (int)len, (const char *)rx_buf);
        }

        for (size_t i = 0; i < len; ++i) {
            sGps.parser.encode((char)rx_buf[i]);
        }
    }
}

bool HAL::GPS_GetInfo(GPS_Info_t *info)
{
    if (info == nullptr) {
        return false;
    }

    memset(info, 0, sizeof(*info));

    info->isVaild = sGps.parser.location.isValid();
    info->longitude = sGps.parser.location.lng();
    info->latitude = sGps.parser.location.lat();
    info->altitude = (float)sGps.parser.altitude.meters();
    info->speed = (float)sGps.parser.speed.kmph();
    info->course = (float)sGps.parser.course.deg();
    info->satellites = (uint8_t)sGps.parser.satellites.value();

    if (sGps.parser.date.isValid() && sGps.parser.time.isValid()) {
        info->clock.year = sGps.parser.date.year();
        info->clock.month = sGps.parser.date.month();
        info->clock.day = sGps.parser.date.day();
        info->clock.hour = sGps.parser.time.hour();
        info->clock.minute = sGps.parser.time.minute();
        info->clock.second = sGps.parser.time.second();
        info->clock.millisecond = sGps.parser.time.centisecond() * 10;
    } else {
        Clock_GetInfo(&info->clock);
    }

    return info->isVaild;
}

bool HAL::GPS_LocationIsValid()
{
    return sGps.parser.location.isValid();
}

double HAL::GPS_GetDistanceOffset(GPS_Info_t *info, double preLong, double preLat)
{
    if (info == nullptr) {
        return 0.0;
    }

    return TinyGPSPlus::distanceBetween(info->latitude, info->longitude, preLat, preLong);
}

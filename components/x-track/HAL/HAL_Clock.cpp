#include "HAL.h"

#include <time.h>

#include "esp_timer.h"

namespace {

int64_t sClockOffsetMs = 0;

}  // namespace

void HAL::Clock_Init()
{
    sClockOffsetMs = 0;
}

void HAL::Clock_GetInfo(Clock_Info_t *info)
{
    if (info == nullptr) {
        return;
    }

    int64_t now_ms = (esp_timer_get_time() / 1000) + sClockOffsetMs;
    time_t sec = (time_t)(now_ms / 1000);
    struct tm t = {};
    localtime_r(&sec, &t);

    info->year = (uint16_t)(t.tm_year + 1900);
    info->month = (uint8_t)(t.tm_mon + 1);
    info->day = (uint8_t)t.tm_mday;
    info->week = (uint8_t)t.tm_wday;
    info->hour = (uint8_t)t.tm_hour;
    info->minute = (uint8_t)t.tm_min;
    info->second = (uint8_t)t.tm_sec;
    info->millisecond = (uint16_t)(now_ms % 1000);
}

void HAL::Clock_SetInfo(const Clock_Info_t *info)
{
    if (info == nullptr) {
        return;
    }

    struct tm t = {};
    t.tm_year = (int)info->year - 1900;
    t.tm_mon = (int)info->month - 1;
    t.tm_mday = info->day;
    t.tm_hour = info->hour;
    t.tm_min = info->minute;
    t.tm_sec = info->second;

    time_t target_sec = mktime(&t);
    if (target_sec == (time_t)-1) {
        return;
    }

    int64_t target_ms = ((int64_t)target_sec * 1000) + info->millisecond;
    int64_t now_ms = esp_timer_get_time() / 1000;
    sClockOffsetMs = target_ms - now_ms;
}

const char *HAL::Clock_GetWeekString(uint8_t week)
{
    static const char *kWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (week > 6) {
        return "N/A";
    }
    return kWeek[week];
}

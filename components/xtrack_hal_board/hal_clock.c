/**
 * @file hal_clock.c
 * @brief Board HAL clock -- system date/time
 *
 * Same contract as the PC simulator implementation, but the ESP32-S3 has no
 * host clock: until SNTP sync or an on-board RTC exists, the system time
 * starts at the build time so the UI shows a plausible, advancing clock.
 *
 * TODO(real): replace with an SNTP-synced clock, or the board RTC
 * (PCF85063A on AuraS3) once a bsp_rtc capability exists.
 */
#include "hal_clock.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* If the system clock is not set yet it reads as ~1970; treat anything
 * before 2020 as "not set". */
#define CLOCK_SET_THRESHOLD 1600000000

static void set_build_time_if_unset(void)
{
    if (time(NULL) >= CLOCK_SET_THRESHOLD)
        return;

    static const char *mon_names[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char mon[4]      = {0};
    int  day = 0, year = 0, hour = 0, minute = 0, second = 0;

    if (sscanf(__DATE__, "%3s %d %d", mon, &day, &year) != 3)
        return;
    if (sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3)
        return;

    int month = 0;
    for (int i = 0; i < 12; i++) {
        if (strncmp(mon, mon_names[i], 3) == 0) {
            month = i;
            break;
        }
    }

    struct tm t    = {0};
    t.tm_year      = year - 1900;
    t.tm_mon       = month;
    t.tm_mday      = day;
    t.tm_hour      = hour;
    t.tm_min       = minute;
    t.tm_sec       = second;

    time_t build_time = mktime(&t);
    if (build_time <= 0)
        return;

    struct timeval tv = {.tv_sec = build_time, .tv_usec = 0};
    settimeofday(&tv, NULL);
}

void hal_clock_init(void)
{
    set_build_time_if_unset();
}

void hal_clock_get_info(hal_clock_info_t *out_info)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t     t  = tv.tv_sec;
    struct tm *lt = localtime(&t);

    out_info->year   = (uint16_t)(lt->tm_year + 1900);
    out_info->month  = (uint8_t)(lt->tm_mon + 1);
    out_info->day    = (uint8_t)lt->tm_mday;
    out_info->hour   = (uint8_t)lt->tm_hour;
    out_info->minute = (uint8_t)lt->tm_min;
    out_info->second = (uint8_t)lt->tm_sec;
    out_info->millis = (uint16_t)(tv.tv_usec / 1000);
}

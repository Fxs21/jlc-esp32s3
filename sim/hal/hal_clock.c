/**
 * @file hal_clock.c
 * @brief HAL clock — system time (localtime)
 */
#include "hal_clock.h"
#include <time.h>
#include <sys/time.h>

void hal_clock_init(void)
{
    /* Nothing to init on desktop — system time is always available */
}

void hal_clock_get_info(hal_clock_info_t *out_info)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t t = tv.tv_sec;
    struct tm *lt = localtime(&t);

    out_info->year   = lt->tm_year + 1900;
    out_info->month  = lt->tm_mon + 1;
    out_info->day    = lt->tm_mday;
    out_info->hour   = lt->tm_hour;
    out_info->minute = lt->tm_min;
    out_info->second = lt->tm_sec;
    out_info->millis = tv.tv_usec / 1000;
}

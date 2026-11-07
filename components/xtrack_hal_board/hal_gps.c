/**
 * @file hal_gps.c
 * @brief Board HAL GPS -- simulated circular track
 *
 * Deliberate simulation so the App can be validated on hardware before the
 * real GNSS path is wired in.  Same math as the PC simulator, driven by
 * esp_timer instead of the host clock: a ~50 m circle, one lap per 120 s.
 *
 * TODO(real): feed real data from the bsp_gnss capability (MAX-M10S on
 * DoerS3) once GNSS is verified on the target board.
 */
#include "hal_gps.h"

#include <math.h>

#include "esp_timer.h"

/* Circle centre (Beijing Olympic Park area) -- same as the PC simulator */
#define CENTER_LAT 39.9900
#define CENTER_LON 116.3910
#define RADIUS_M   50.0
#define PERIOD_SEC 120

/*
 * Approximate metre-to-degree conversions at lat 40N:
 *   1 deg latitude  ~ 111320 m
 *   1 deg longitude ~ 111320 * cos(40 deg) ~ 85200 m
 */
#define M_TO_LAT(m) ((m) / 111320.0)
#define M_TO_LON(m) ((m) / 85200.0)

void hal_gps_get_info(hal_gps_info_t *out_info)
{
    double uptime_s = (double)esp_timer_get_time() / 1000000.0;
    double t        = fmod(uptime_s, (double)PERIOD_SEC);
    double angle    = (t / (double)PERIOD_SEC) * 2.0 * M_PI;

    /* Position on circle */
    double dx = cos(angle) * (double)RADIUS_M;
    double dy = sin(angle) * (double)RADIUS_M;

    out_info->latitude  = CENTER_LAT + M_TO_LAT(dy);
    out_info->longitude = CENTER_LON + M_TO_LON(dx);
    out_info->altitude  = 50.0f;

    /* Speed: circumference / period */
    out_info->speed = (float)(2.0 * M_PI * (double)RADIUS_M / (double)PERIOD_SEC * 3.6);

    /* Course: direction of travel (perpendicular to radius) */
    out_info->course = (float)fmod((angle * 180.0 / M_PI) + 90.0, 360.0);

    out_info->satellites = 8;
    out_info->is_valid   = 1;
}

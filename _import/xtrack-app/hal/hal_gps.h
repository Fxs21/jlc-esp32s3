/**
 * @file  hal_gps.h
 * @brief HAL abstraction for GPS module
 */
#ifndef HAL_GPS_H
#define HAL_GPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  GPS information structure
 */
typedef struct {
    double longitude;      /**< Longitude (degrees) */
    double latitude;       /**< Latitude (degrees) */
    float  altitude;       /**< Altitude (m) */
    float  speed;          /**< Speed (km/h) */
    float  course;         /**< Course (degrees, 0-360) */
    int16_t satellites;    /**< Number of satellites in view */
    uint8_t is_valid;      /**< Non-zero if GPS fix is valid */
} hal_gps_info_t;

/**
 * @brief  Get current GPS position data
 * @param  out_info: Output structure filled with current GPS data
 */
void hal_gps_get_info(hal_gps_info_t *out_info);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPS_H */

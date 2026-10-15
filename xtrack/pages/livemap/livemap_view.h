/**
 * @file  livemap_view.h
 * @brief LiveMap View — map background with sport info overlay (X-Track style)
 */
#ifndef LIVEMAP_VIEW_H
#define LIVEMAP_VIEW_H

#include <lvgl.h>

typedef struct {
    struct {
        lv_obj_t *cont;          /**< Dark semi-transparent container */
        lv_obj_t *label_info;    /**< "LOADING..." placeholder */

        /* Speed (left side) */
        lv_obj_t *label_speed;   /**< Large speed number */
        lv_obj_t *label_unit;    /**< "km/h" below speed */

        /* Trip (right side, icon + label rows) */
        lv_obj_t *img_trip;
        lv_obj_t *label_trip;
        lv_obj_t *img_alarm;
        lv_obj_t *label_time;
    } sport_info;

    /* Map area */
    struct {
        lv_obj_t *cont;         /**< Map container (full page area) */
        lv_obj_t *img_arrow;    /**< GPS direction arrow (centred) */
        lv_obj_t *line_track;   /**< GPS track line (orange) */
    } map;

    /* Track point buffer */
    lv_point_precise_t track_points[512];
    int track_count;
} livemap_view_t;

void livemap_view_create(livemap_view_t *view, lv_obj_t *root);
void livemap_view_delete(livemap_view_t *view);

/** Set speed value (e.g. "00") */
void livemap_view_set_speed(livemap_view_t *view, const char *speed);

/** Set trip distance (e.g. "0.0 km") */
void livemap_view_set_trip(livemap_view_t *view, const char *trip);

/** Set elapsed time (e.g. "00:00") */
void livemap_view_set_time(livemap_view_t *view, const char *time);

/** Add a track point (screen coordinates) */
void livemap_view_add_track_point(livemap_view_t *view, lv_point_precise_t pt);

/** Clear all track points */
void livemap_view_clear_track(livemap_view_t *view);

#endif /* LIVEMAP_VIEW_H */

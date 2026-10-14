/**
 * @file  startup_view.h
 * @brief Startup View — splash screen UI (no data logic)
 */
#ifndef STARTUP_VIEW_H
#define STARTUP_VIEW_H

#include <lvgl.h>

typedef struct {
    lv_obj_t *cont;            /**< Centered container with bottom border */
    lv_obj_t *label_logo;      /**< "X-TRACK" text */

    lv_anim_timeline_t *anim_timeline; /**< Entry animation timeline */
} startup_view_t;

/** Create all startup widgets under root */
void startup_view_create(startup_view_t *view, lv_obj_t *root);

/** Destroy startup widgets and animation timeline */
void startup_view_delete(startup_view_t *view);

#endif /* STARTUP_VIEW_H */

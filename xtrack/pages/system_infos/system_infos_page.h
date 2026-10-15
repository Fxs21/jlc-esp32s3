/**
 * @file  system_infos_page.h
 * @brief SystemInfos page — Presenter (wires View + Model)
 *
 * Displays real-time system data (GPS, Power) received
 * from DataProc nodes via the DataCenter.
 *
 * Architecture (X-Track pattern):
 *   page_system_infos_t (Presenter)
 *     +-- system_infos_view_t  (pure UI)
 *     +-- system_infos_model_t (Account + data)
 */
#ifndef SYSTEM_INFOS_PAGE_H
#define SYSTEM_INFOS_PAGE_H

#include "page_manager/page.h"
#include "system_infos_view.h"
#include "system_infos_model.h"

typedef struct page_system_infos_t {
    page_t base;

    system_infos_view_t view;   /**< UI widgets */
    system_infos_model_t model; /**< Data subscription */

    lv_timer_t *timer;          /**< 1s update timer */
    int   trip_time;            /**< Trip time (seconds) */
    float max_speed;            /**< Max speed recorded */
    float trip_distance;        /**< Trip distance (km) */
    lv_obj_t *last_focus;       /**< Last focused icon (for click-to-pop) */
} page_system_infos_t;

/** Initialise the system infos page (Presenter) */
void page_system_infos_init(page_system_infos_t *p);

#endif /* SYSTEM_INFOS_PAGE_H */

/**
 * @file  system_infos_view.h
 * @brief SystemInfos View — focusable card list with icon shrink animation
 */
#ifndef SYSTEM_INFOS_VIEW_H
#define SYSTEM_INFOS_VIEW_H

#include <lvgl.h>

typedef struct {
    lv_obj_t *cont;         /**< Card container */
    lv_obj_t *icon;         /**< Left sidebar (width animates 220→70 on focus) */
    lv_obj_t *label_info;   /**< Field names (multi-line, e.g. "Latitude\nLongitude") */
    lv_obj_t *label_data;   /**< Field values (multi-line, e.g. "39.9\n116.4") */
} system_infos_item_t;

typedef struct {
    system_infos_item_t sport;
    system_infos_item_t gps;
    system_infos_item_t mag;
    system_infos_item_t imu;
    system_infos_item_t rtc;
    system_infos_item_t battery;
    system_infos_item_t storage;
    system_infos_item_t system;
} system_infos_view_t;

void system_infos_view_create(system_infos_view_t *view, lv_obj_t *root);
void system_infos_view_delete(system_infos_view_t *view);

/** Set multi-line data text on an item's value label */
void system_infos_view_set_data(system_infos_item_t *item, const char *data);

/** (Re)build group: remove stale objects, add all items */
void system_infos_view_group_init(system_infos_view_t *view, lv_group_t *g);

/** Remove all items from group (cleanup on disappear) */
void system_infos_view_group_deinit(system_infos_view_t *view, lv_group_t *g);

#endif /* SYSTEM_INFOS_VIEW_H */

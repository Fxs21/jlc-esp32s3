/**
 * @file  dialplate_view.h
 * @brief Dialplate View — X-Track style layout
 *
 * Layout (top to bottom):
 *   topInfo     — Speed + unit (km/h), rounded dark container
 *   bottomInfo  — 4 sub-info groups flex row: AVG | Time | Trip | Calorie
 *   btnCont     — 3 centered buttons: Map / Record / Menu
 */
#ifndef DIALPLATE_VIEW_H
#define DIALPLATE_VIEW_H

#include <lvgl.h>

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *label_value; /**< Number value */
    lv_obj_t *label_unit;  /**< Unit text below */
} dialplate_sub_info_t;

typedef struct {
    struct {
        lv_obj_t *cont;
        lv_obj_t *label_speed; /**< Large speed number */
        lv_obj_t *label_unit;  /**< "km/h" */
    } top_info;

    struct {
        lv_obj_t *cont;
        dialplate_sub_info_t grp[4]; /**< AVG, Time, Trip, Calorie */
    } bottom_info;

    struct {
        lv_obj_t *cont;
        lv_obj_t *btn_map;  /**< Map / locate */
        lv_obj_t *btn_rec;  /**< Record / start */
        lv_obj_t *btn_menu; /**< Menu / system */
    } btn_cont;
} dialplate_view_t;

/** Create all dialplate widgets under root */
void dialplate_view_create(dialplate_view_t *view, lv_obj_t *root);

/** Switch record button image (start / pause / stop) */
void dialplate_view_set_rec_img(lv_obj_t *btn_rec, const char *img_name);

/** Destroy all dialplate widgets */
void dialplate_view_delete(dialplate_view_t *view);

#endif /* DIALPLATE_VIEW_H */

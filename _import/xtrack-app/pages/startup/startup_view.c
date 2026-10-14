/**
 * @file startup_view.c
 * @brief Startup View — splash with orange border + entry animation
 */
#include "startup_view.h"
#include <stdlib.h>

#define COLOR_ORANGE lv_color_hex(0xff931e)

void startup_view_create(startup_view_t *view, lv_obj_t *root)
{
    /* Centered container: 110 x 50, orange bottom border, no scroll */
    view->cont = lv_obj_create(root);
    lv_obj_remove_style_all(view->cont);
    lv_obj_clear_flag(view->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(view->cont, 140, 50);
    lv_obj_set_style_border_color(view->cont, COLOR_ORANGE, 0);
    lv_obj_set_style_border_side(view->cont, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(view->cont, 3, 0);
    lv_obj_set_style_border_post(view->cont, true, 0);
    lv_obj_center(view->cont);

    /* Logo label */
    view->label_logo = lv_label_create(view->cont);
    lv_label_set_text(view->label_logo, "X-TRACK");
    lv_obj_set_style_text_font(view->label_logo, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(view->label_logo, lv_color_white(), 0);
    lv_obj_center(view->label_logo);
    /* Save centered y before moving off-screen for animation */
    lv_coord_t y_center = lv_obj_get_y(view->label_logo);
    /* Start below container so animation slides it up */
    lv_obj_set_y(view->label_logo, lv_obj_get_style_height(view->cont, 0));

    /* Entry animation timeline: two animations in sequence */
    view->anim_timeline = lv_anim_timeline_create();

    /* Animation 1: container width expands from 0 to 110 (starts at 0ms) */
    lv_anim_t a_cont;
    lv_anim_init(&a_cont);
    lv_anim_set_var(&a_cont, view->cont);
    lv_anim_set_exec_cb(&a_cont, (lv_anim_exec_xcb_t)lv_obj_set_width);
    lv_anim_set_values(&a_cont, 0, lv_obj_get_style_width(view->cont, 0));
    lv_anim_set_time(&a_cont, 500);
    lv_anim_set_path_cb(&a_cont, lv_anim_path_ease_out);
    lv_anim_timeline_add(view->anim_timeline, 0, &a_cont);

    /* Animation 2: label slides up into container (starts at 500ms) */
    lv_anim_t a_label;
    lv_anim_init(&a_label);
    lv_anim_set_var(&a_label, view->label_logo);
    lv_anim_set_exec_cb(&a_label, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a_label, lv_obj_get_style_height(view->cont, 0),
                       y_center);
    lv_anim_set_time(&a_label, 500);
    lv_anim_set_path_cb(&a_label, lv_anim_path_ease_out);
    lv_anim_timeline_add(view->anim_timeline, 500, &a_label);
}

void startup_view_delete(startup_view_t *view)
{
    if (view->anim_timeline) {
        lv_anim_timeline_delete(view->anim_timeline);
        view->anim_timeline = NULL;
    }

    lv_obj_delete(view->label_logo);
    lv_obj_delete(view->cont);

    view->label_logo = NULL;
    view->cont       = NULL;
}

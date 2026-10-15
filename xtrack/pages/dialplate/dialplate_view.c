/**
 * @file dialplate_view.c
 * @brief Dialplate View — X-Track style layout with icon buttons
 */
#include "dialplate_view.h"
#include "resource_pool.h"
#include <stdlib.h>
#include <string.h>

/* Font size mapping (X-Track → stock Montserrat):
 *   bahnschrift_65 → montserrat_48 (closest large)
 *   bahnschrift_17 → montserrat_16
 *   bahnschrift_13 → montserrat_14
 */
#define FONT_SPEED  &lv_font_montserrat_48
#define FONT_VALUE  &lv_font_montserrat_16
#define FONT_UNIT   &lv_font_montserrat_14

#define COLOR_BG_TOP   lv_color_hex(0x333333)
#define COLOR_UNIT     lv_color_hex(0xb3b3b3)
#define COLOR_BTN_BG   lv_color_hex(0x666666)
#define COLOR_BTN_PRS  lv_color_hex(0xbbbbbb)

static void sub_info_create(lv_obj_t *parent, dialplate_sub_info_t *info,
                            const char *unit_text)
{
    info->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(info->cont);
    lv_obj_set_size(info->cont, 93, 39);

    lv_obj_set_flex_flow(info->cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info->cont, LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    info->label_value = lv_label_create(info->cont);
    lv_obj_set_style_text_font(info->label_value, FONT_VALUE, 0);
    lv_obj_set_style_text_color(info->label_value, lv_color_white(), 0);
    lv_label_set_text(info->label_value, "--");

    info->label_unit = lv_label_create(info->cont);
    lv_obj_set_style_text_font(info->label_unit, FONT_UNIT, 0);
    lv_obj_set_style_text_color(info->label_unit, COLOR_UNIT, 0);
    lv_label_set_text(info->label_unit, unit_text);
}

static lv_obj_t *btn_create(lv_obj_t *parent, const char *img_name,
                            lv_coord_t x_ofs)
{
    const lv_img_dsc_t *img = resource_pool_get_image(img_name);
    if (!img)
        return NULL;

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, 40, 31);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(obj, LV_ALIGN_CENTER, x_ofs, 0);

    lv_obj_set_style_bg_img_src(obj, img, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, COLOR_BTN_BG, 0);
    lv_obj_set_style_bg_color(obj, COLOR_BTN_PRS, LV_STATE_PRESSED);
    lv_obj_set_style_radius(obj, 9, 0);
    lv_obj_set_style_width(obj, 45, LV_STATE_PRESSED);
    lv_obj_set_style_height(obj, 25, LV_STATE_PRESSED);

    return obj;
}

void dialplate_view_create(dialplate_view_t *view, lv_obj_t *root)
{
    lv_obj_t *cont = lv_obj_create(root);
    lv_obj_remove_style_all(cont);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_size(cont, LV_HOR_RES, 90);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 106);

    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    view->bottom_info.cont = cont;

    static const char *units[4] = {"AVG", "Time", "Trip", "Cal"};
    for (int i = 0; i < 4; i++) {
        sub_info_create(cont, &view->bottom_info.grp[i], units[i]);
    }

    cont = lv_obj_create(root);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, 142);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cont, COLOR_BG_TOP, 0);
    lv_obj_set_style_radius(cont, 27, 0);
    lv_obj_set_y(cont, -36);
    view->top_info.cont = cont;

    view->top_info.label_speed = lv_label_create(cont);
    lv_obj_set_style_text_font(view->top_info.label_speed, FONT_SPEED, 0);
    lv_obj_set_style_text_color(view->top_info.label_speed, lv_color_white(), 0);
    lv_label_set_text(view->top_info.label_speed, "--");
    lv_obj_align(view->top_info.label_speed, LV_ALIGN_TOP_MID, 0, 63);

    view->top_info.label_unit = lv_label_create(cont);
    lv_obj_set_style_text_font(view->top_info.label_unit, FONT_VALUE, 0);
    lv_obj_set_style_text_color(view->top_info.label_unit, lv_color_white(), 0);
    lv_label_set_text(view->top_info.label_unit, "km/h");
    lv_obj_align_to(view->top_info.label_unit, view->top_info.label_speed,
                    LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    cont = lv_obj_create(root);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, 40);
    lv_obj_align_to(cont, view->bottom_info.cont, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    view->btn_cont.cont = cont;

    view->btn_cont.btn_map  = btn_create(cont, "locate", -80);
    view->btn_cont.btn_rec  = btn_create(cont, "start",   0);
    view->btn_cont.btn_menu = btn_create(cont, "menu",   80);
}

void dialplate_view_set_rec_img(lv_obj_t *btn_rec, const char *img_name)
{
    const lv_img_dsc_t *img = resource_pool_get_image(img_name);
    if (img)
        lv_obj_set_style_bg_img_src(btn_rec, img, 0);
}

void dialplate_view_delete(dialplate_view_t *view)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_delete(view->bottom_info.grp[i].label_value);
        lv_obj_delete(view->bottom_info.grp[i].label_unit);
        lv_obj_delete(view->bottom_info.grp[i].cont);
    }
    lv_obj_delete(view->bottom_info.cont);

    lv_obj_delete(view->top_info.label_speed);
    lv_obj_delete(view->top_info.label_unit);
    lv_obj_delete(view->top_info.cont);

    lv_obj_delete(view->btn_cont.btn_map);
    lv_obj_delete(view->btn_cont.btn_rec);
    lv_obj_delete(view->btn_cont.btn_menu);
    lv_obj_delete(view->btn_cont.cont);

    memset(view, 0, sizeof(*view));
}

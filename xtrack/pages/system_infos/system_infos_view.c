/**
 * @file system_infos_view.c
 * @brief SystemInfos View — focusable card list (X-Track style)
 *
 * Each item is a card (220px wide, variable height).  Left sidebar (icon
 * area) animates width 220→70 on focus with overshoot + orange border.
 *
 * Width is managed entirely by lv_obj_set_width (LOCAL style), NOT by
 * state-based styles (s_style_focus).  In LVGL v9, lv_anim_t's exec
 * callback lv_obj_set_width creates a LOCAL style entry whose priority
 * exceeds state styles, so putting width in LV_STATE_FOCUSED has no
 * visible effect — lv_obj_get_width always returns the LOCAL value,
 * making the animation start === target (invisible).  The two state
 * styles (s_style_icon, s_style_focus) therefore only carry non-width
 * properties (bg/font/border).
 */
#include "system_infos_view.h"
#include "resource_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITEM_HEIGHT_MIN 100
#define ITEM_PAD        ((LV_VER_RES - ITEM_HEIGHT_MIN) / 2)

#define CARD_W     220
#define ICON_W     220
#define ICON_W_FOCUS 70

#define COLOR_ORANGE lv_color_hex(0xff931e)

/* Styles */
static lv_style_t s_style_icon;
static lv_style_t s_style_focus;
static lv_style_t s_style_info;
static lv_style_t s_style_data;
static bool       s_styles_inited = false;

static void style_init(void)
{
    if (s_styles_inited)
        return;
    s_styles_inited = true;

    lv_style_init(&s_style_icon);
    lv_style_set_bg_color(&s_style_icon, lv_color_black());
    lv_style_set_bg_opa(&s_style_icon, LV_OPA_COVER);
    lv_style_set_text_font(&s_style_icon, &lv_font_montserrat_16);
    lv_style_set_text_color(&s_style_icon, lv_color_white());

    lv_style_init(&s_style_focus);
    lv_style_set_border_side(&s_style_focus, LV_BORDER_SIDE_RIGHT);
    lv_style_set_border_width(&s_style_focus, 2);
    lv_style_set_border_color(&s_style_focus, COLOR_ORANGE);

    lv_style_init(&s_style_info);
    lv_style_set_text_font(&s_style_info, &lv_font_montserrat_14);
    lv_style_set_text_color(&s_style_info, lv_color_hex(0x999999));

    lv_style_init(&s_style_data);
    lv_style_set_text_font(&s_style_data, &lv_font_montserrat_14);
    lv_style_set_text_color(&s_style_data, lv_color_white());
}

/* LVGL v9: group only sends LV_EVENT_FOCUSED/DEFOCUSED but does NOT
 * set LV_STATE_FOCUSED on the object.  Width animation is handled by
 * explicit lv_anim_t (style transition removed to avoid conflict). */
static void on_icon_focus_event(lv_event_t *e)
{
    lv_obj_t *icon = lv_event_get_current_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        lv_obj_add_state(icon, LV_STATE_FOCUSED);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, icon);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
        lv_anim_set_values(&a, lv_obj_get_width(icon), ICON_W_FOCUS);
        lv_anim_set_time(&a, 200);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_start(&a);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_clear_state(icon, LV_STATE_FOCUSED);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, icon);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
        lv_anim_set_values(&a, lv_obj_get_width(icon), ICON_W);
        lv_anim_set_time(&a, 200);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_start(&a);
    }
}

static void item_create(system_infos_item_t *item, lv_obj_t *parent,
                        const char *name, const char *img_name,
                        const char *info_text)
{
    const lv_img_dsc_t *img_dsc = resource_pool_get_image(img_name);
    if (!img_dsc)
        return;
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_enable_style_refresh(false);
    lv_obj_remove_style_all(cont);
    lv_obj_set_width(cont, CARD_W);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    item->cont = cont;

    /* Icon sidebar */
    lv_obj_t *icon = lv_obj_create(cont);
    lv_obj_enable_style_refresh(false);
    lv_obj_remove_style_all(icon);
    lv_obj_set_width(icon, ICON_W);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_style(icon, &s_style_icon, 0);
    lv_obj_add_style(icon, &s_style_focus, LV_STATE_FOCUSED);
    lv_obj_set_style_align(icon, LV_ALIGN_LEFT_MID, 0);
    lv_obj_set_flex_flow(icon, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(icon, LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    item->icon = icon;

    /* LVGL v9: group does NOT set LV_STATE_FOCUSED automatically,
     * only sends LV_EVENT_FOCUSED.  This handler applies the state. */
    lv_obj_add_event_cb(icon, on_icon_focus_event, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(icon, on_icon_focus_event, LV_EVENT_DEFOCUSED, NULL);

    /* Image */
    lv_obj_t *img = lv_img_create(icon);
    lv_obj_enable_style_refresh(false);
    lv_img_set_src(img, img_dsc);

    /* Name */
    lv_obj_t *lbl = lv_label_create(icon);
    lv_obj_enable_style_refresh(false);
    lv_label_set_text(lbl, name);

    /* Info labels */
    lbl = lv_label_create(cont);
    lv_obj_enable_style_refresh(false);
    lv_label_set_text(lbl, info_text);
    lv_obj_add_style(lbl, &s_style_info, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 75, 0);
    item->label_info = lbl;

    /* Data labels */
    lbl = lv_label_create(cont);
    lv_obj_enable_style_refresh(false);
    lv_label_set_text(lbl, "--");
    lv_obj_add_style(lbl, &s_style_data, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 60, 0);
    item->label_data = lbl;

    lv_obj_move_foreground(icon);
    lv_obj_enable_style_refresh(true);

    lv_obj_update_layout(item->label_info);
    lv_coord_t h = lv_obj_get_height(item->label_info);
    h = LV_MAX(h, ITEM_HEIGHT_MIN);
    lv_obj_set_height(cont, h);
    lv_obj_set_height(icon, h);
}

void system_infos_view_group_init(system_infos_view_t *view, lv_group_t *g)
{
    if (!g)
        return;

    /* Remove any stale objects from group (history pollution) */
    lv_group_remove_all_objs(g);

    /* Add items in reverse order (encoder comfort) */
    system_infos_item_t *items = (system_infos_item_t *)&view->sport;
    int n = sizeof(system_infos_view_t) / sizeof(system_infos_item_t);
    for (int i = n - 1; i >= 0; i--)
        lv_group_add_obj(g, items[i].icon);

    /* Focus to LAST item so that on_will_appear's focus to
     * items[0] (Sport) is a real change and triggers events. */
    lv_group_focus_obj(items[n - 1].icon);
}

void system_infos_view_group_deinit(system_infos_view_t *view, lv_group_t *g)
{
    if (!g)
        return;

    system_infos_item_t *items = (system_infos_item_t *)&view->sport;
    int n = sizeof(system_infos_view_t) / sizeof(system_infos_item_t);
    for (int i = 0; i < n; i++)
        lv_group_remove_obj(items[i].icon);
}

void system_infos_view_create(system_infos_view_t *view, lv_obj_t *root)
{
    style_init();

    lv_obj_set_style_pad_ver(root, ITEM_PAD, 0);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    item_create(&view->sport,   root, "Sport",   "bicycle",
                "Total trip\nTotal time\nMax speed");
    item_create(&view->gps,     root, "GPS",     "map_location",
                "Latitude\nLongitude\nAltitude\nCourse\nSpeed");
    item_create(&view->mag,     root, "MAG",     "compass",
                "Dir\nX\nY\nZ");
    item_create(&view->imu,     root, "IMU",     "gyroscope",
                "Step\nInfo");
    item_create(&view->rtc,     root, "RTC",     "time_info",
                "Date\nTime");
    item_create(&view->battery, root, "Battery", "battery_info",
                "Usage\nVoltage\nStatus");
    item_create(&view->storage, root, "Storage", "storage",
                "Status\nSize\nType\nVersion");
    item_create(&view->system,  root, "System",  "system_info",
                "Firmware\nLVGL\nBoot\nCompiler\nBuild");
}

void system_infos_view_delete(system_infos_view_t *view)
{
    system_infos_item_t *items = (system_infos_item_t *)&view->sport;
    int n = sizeof(system_infos_view_t) / sizeof(system_infos_item_t);
    for (int i = 0; i < n; i++)
        lv_obj_delete(items[i].cont);
    memset(view, 0, sizeof(*view));
}

void system_infos_view_set_data(system_infos_item_t *item, const char *data)
{
    lv_label_set_text(item->label_data, data);
}

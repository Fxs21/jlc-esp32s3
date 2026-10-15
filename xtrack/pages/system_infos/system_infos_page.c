/**
 * @file system_infos_page.c
 * @brief SystemInfos page — focusable card list (X-Track pattern)
 *
 * X-Track flow:
 *   on_load:           create view, model, timer (no group setup)
 *   on_will_appear:    rebuild group, focus first (icon→70px),
 *                      scroll to -LV_VER_RES, fade_in
 *   on_did_appear:     scroll to first item with animation
 *   on_did_disappear:  clear group (prevent history pollution)
 */
#include <inttypes.h>
#include "system_infos_page.h"
#include "app.h"
#include "page_manager/page_manager.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TAG "system_infos"
#define UPDATE_MS 1000

static uint32_t s_boot_tick = 0;
static int s_mag_tick = 0;

static void on_focus_cb(lv_group_t *g)
{
    lv_obj_t *icon = lv_group_get_focused(g);
    if (!icon)
        return;
    lv_obj_t *cont = lv_obj_get_parent(icon);
    lv_coord_t y   = lv_obj_get_y(cont);
    lv_obj_scroll_to_y(lv_obj_get_parent(cont), y, LV_ANIM_ON);
}

static void on_item_press(lv_event_t *e)
{
    page_t *page = (page_t *)lv_event_get_user_data(e);
    lv_obj_t *icon = lv_event_get_current_target(e);
    page_system_infos_t *p = (page_system_infos_t *)page;

    if (p->last_focus == icon) {
        pm_pop(page->manager);
    } else {
        p->last_focus = icon;
        lv_obj_t *cont = lv_obj_get_parent(icon);
        lv_coord_t y   = lv_obj_get_y(cont);
        lv_obj_scroll_to_y(lv_obj_get_parent(cont), y, LV_ANIM_ON);
        lv_group_focus_obj(icon);
    }
}

static void on_root_click(lv_event_t *e)
{
    page_t *page = (page_t *)lv_event_get_user_data(e);
    pm_pop(page->manager);
}

static void update_view(page_system_infos_t *p)
{
    hal_gps_info_t   *gps = &p->model.gps_info;
    hal_power_info_t *pwr = &p->model.power_info;
    char buf[192];

    snprintf(buf, sizeof(buf), "%.2f km\n%02d:%02d\n%.1f km/h",
             p->trip_distance, p->trip_time / 60, p->trip_time % 60,
             (double)p->max_speed);
    system_infos_view_set_data(&p->view.sport, buf);

    if (gps->is_valid)
        snprintf(buf, sizeof(buf), "%.6f\n%.6f\n%.0f m\n%.0f deg\n%.1f km/h",
                 gps->latitude, gps->longitude,
                 (double)gps->altitude, (double)gps->course, (double)gps->speed);
    else
        snprintf(buf, sizeof(buf), "--\n--\n--\n--\n--");
    system_infos_view_set_data(&p->view.gps, buf);

    s_mag_tick++;
    float dir = fmodf(s_mag_tick * 7.3f, 360.0f);
    int x = (rand() % 2001) - 1000;
    int y = (rand() % 2001) - 1000;
    int z = (rand() % 2001) - 1000;
    snprintf(buf, sizeof(buf), "%.1f deg\n%d\n%d\n%d", (double)dir, x, y, z);
    system_infos_view_set_data(&p->view.mag, buf);

    int step = (lv_tick_get() / 100) % 10000;
    snprintf(buf, sizeof(buf), "%d\nOK", step);
    system_infos_view_set_data(&p->view.imu, buf);

    uint32_t sec = lv_tick_get() / 1000;
    snprintf(buf, sizeof(buf),
             "%04d-%02d-%02d\n%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
             2026, 6, 12, (sec / 3600) % 24, (sec / 60) % 60, sec % 60);
    system_infos_view_set_data(&p->view.rtc, buf);

    snprintf(buf, sizeof(buf), "%d%%\n%.2f V\n%s",
             pwr->percentage, (double)pwr->voltage / 1000.0,
             pwr->is_charging ? "Charging" : "Discharging");
    system_infos_view_set_data(&p->view.battery, buf);

    system_infos_view_set_data(&p->view.storage, "OK\n128 MB\nFAT32\nv2.0");

    uint32_t boot_sec = (lv_tick_get() - s_boot_tick) / 1000;
    snprintf(buf, sizeof(buf),
             "v1.0\nLVGL 9.6\n%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "\nGCC\n2025-01-01",
             boot_sec / 3600, (boot_sec / 60) % 60, boot_sec % 60);
    system_infos_view_set_data(&p->view.system, buf);
}

static void on_timer(lv_timer_t *timer)
{
    page_system_infos_t *p = (page_system_infos_t *)lv_timer_get_user_data(timer);
    account_pull(p->model.account, "GPS", &p->model.gps_info,
                 sizeof(p->model.gps_info));
    account_pull(p->model.account, "Power", &p->model.power_info,
                 sizeof(p->model.power_info));
    p->trip_time += UPDATE_MS / 1000;
    if (p->model.gps_info.is_valid &&
        p->model.gps_info.speed > p->max_speed)
        p->max_speed = p->model.gps_info.speed;
    update_view(p);
}

static void on_load(page_t *base)
{
    page_system_infos_t *p = (page_system_infos_t *)base;

    system_infos_view_create(&p->view, base->root);

    if (s_boot_tick == 0)
        s_boot_tick = lv_tick_get();

    system_infos_model_init(&p->model, g_data_center);

    account_pull(p->model.account, "GPS", &p->model.gps_info,
                 sizeof(p->model.gps_info));
    account_pull(p->model.account, "Power", &p->model.power_info,
                 sizeof(p->model.power_info));
    update_view(p);

    p->timer = lv_timer_create(on_timer, UPDATE_MS, p);

    /* Item press events */
    system_infos_item_t *items = (system_infos_item_t *)&p->view.sport;
    int n = sizeof(system_infos_view_t) / sizeof(system_infos_item_t);
    for (int i = 0; i < n; i++)
        lv_obj_add_event_cb(items[i].icon, on_item_press, LV_EVENT_PRESSED, base);

    /* Root click = pop */
    lv_obj_add_event_cb(base->root, on_root_click, LV_EVENT_CLICKED, base);
}

static void on_will_appear(page_t *base)
{
    page_system_infos_t *p = (page_system_infos_t *)base;
    lv_group_t *g = lv_group_get_default();

    /* Rebuild group: clean stale state, add all items */
    system_infos_view_group_init(&p->view, g);
    lv_group_set_focus_cb(g, on_focus_cb);

    /* Scroll off-screen BEFORE focus change. */
    lv_obj_scroll_to_y(base->root, -LV_VER_RES, LV_ANIM_OFF);

    /* Focus first item — event handler triggers width animation */
    system_infos_item_t *items = (system_infos_item_t *)&p->view.sport;
    p->last_focus = items[0].icon;
    lv_group_focus_obj(items[0].icon);
    lv_obj_add_state(items[0].icon, LV_STATE_FOCUSED);
}

static void on_did_appear(page_t *base)
{
    /* Scroll to first item with animation — page is visible now */
    page_system_infos_t *p = (page_system_infos_t *)base;
    system_infos_item_t *items = (system_infos_item_t *)&p->view.sport;
    lv_obj_t *cont = lv_obj_get_parent(items[0].icon);
    lv_coord_t y = lv_obj_get_y(cont);
    lv_obj_scroll_to_y(lv_obj_get_parent(cont), y, LV_ANIM_ON);
}

static void on_did_disappear(page_t *base)
{
    page_system_infos_t *p = (page_system_infos_t *)base;

    /* Clean group: remove items to prevent history pollution */
    lv_group_t *g = lv_group_get_default();
    system_infos_view_group_deinit(&p->view, g);
    lv_group_set_focus_cb(g, NULL);
}

static void on_did_unload(page_t *base)
{
    page_system_infos_t *p = (page_system_infos_t *)base;

    if (p->timer) {
        lv_timer_delete(p->timer);
        p->timer = NULL;
    }

    system_infos_model_deinit(&p->model);
    system_infos_view_delete(&p->view);
}

void page_system_infos_init(page_system_infos_t *p)
{
    memset(p, 0, sizeof(*p));
    page_vtable_t vtable = {
        .on_load = on_load,
        .on_will_appear = on_will_appear,
        .on_did_appear = on_did_appear,
        .on_did_disappear = on_did_disappear,
        .on_did_unload = on_did_unload,
    };
    page_init(&p->base, "SystemInfos", vtable);
}

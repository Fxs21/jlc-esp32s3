/**
 * @file dialplate_page.c
 * @brief Dialplate page -- Presenter (bridges View and Model)
 *
 * Lifecycle (X-Track pattern):
 *   on_load          -> View.Create + Model.Init + initial pull
 *   on_will_appear   -> start periodic sport-info timer (100ms)
 *   on_did_disappear -> stop timer
 *   Model callback   -> update_view() on every DataProc publish
 *   on_did_unload    -> Model.Deinit + View.Delete
 */
#include "dialplate_page.h"
#include "app.h"
#include "page_manager/page_manager.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TAG "dialplate"

/* Sport info update interval */
#define SPORT_UPDATE_MS 500

/* Private data stored per page instance */
typedef struct {
    lv_timer_t *timer;        /**< Periodic sport-info update timer */
    int   prev_sec;            /**< Previous trip second (for time calc) */
    float prev_lat;            /**< Previous latitude (for distance calc) */
    float prev_lon;            /**< Previous longitude (for distance calc) */
    bool  has_prev_pos;        /**< Whether prev_lat/lon are valid */
} dialplate_priv_t;

static void update_sport_info(page_dialplate_t *p, dialplate_priv_t *priv)
{
    hal_gps_info_t *gps = &p->model.gps_info;

    /* Top speed */
    if (gps->is_valid) {
        lv_label_set_text_fmt(p->view.top_info.label_speed, "%.0f",
                              (double)gps->speed);
    } else {
        lv_label_set_text(p->view.top_info.label_speed, "--");
    }

    /* Bottom info: AVG / Time / Trip / Cal */
    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f", (double)p->model.avg_speed);
    lv_label_set_text(p->view.bottom_info.grp[0].label_value, buf);

    snprintf(buf, sizeof(buf), "%02d:%02d",
             p->model.trip_time_sec / 60, p->model.trip_time_sec % 60);
    lv_label_set_text(p->view.bottom_info.grp[1].label_value, buf);

    snprintf(buf, sizeof(buf), "%.1f", (double)p->model.trip_distance);
    lv_label_set_text(p->view.bottom_info.grp[2].label_value, buf);

    snprintf(buf, sizeof(buf), "%d", p->model.calories);
    lv_label_set_text(p->view.bottom_info.grp[3].label_value, buf);
}

/* Called when GPS position changes — update trip distance */
static void on_position_change(page_dialplate_t *p, dialplate_priv_t *priv)
{
    hal_gps_info_t *gps = &p->model.gps_info;
    if (!gps->is_valid)
        return;

    if (priv->has_prev_pos) {
        /* Approximate distance using Haversine (simplified) */
        float dlat = (gps->latitude - priv->prev_lat) * 111320.0f;
        float dlon = (gps->longitude - priv->prev_lon) * 111320.0f
                     * cosf(gps->latitude * 3.14159f / 180.0f);
        float dist = sqrtf(dlat * dlat + dlon * dlon) / 1000.0f; /* km */
        if (dist > 0.001f)
            p->model.trip_distance += dist;
    }

    priv->prev_lat   = (float)gps->latitude;
    priv->prev_lon   = (float)gps->longitude;
    priv->has_prev_pos = true;
}

static int on_data_arrived(dialplate_model_t *m, account_event_param_t *param)
{
    page_dialplate_t *p =
        (page_dialplate_t *)((char *)m - offsetof(page_dialplate_t, model));
    dialplate_priv_t *priv = (dialplate_priv_t *)p->base.user_data;

    /* When GPS data arrives, update trip distance from position change */
    if (strcmp(param->tran->id, "GPS") == 0)
        on_position_change(p, priv);

    return ACCOUNT_OK;
}

static void on_sport_timer(lv_timer_t *timer)
{
    page_dialplate_t *p = (page_dialplate_t *)lv_timer_get_user_data(timer);
    dialplate_priv_t *priv = (dialplate_priv_t *)p->base.user_data;

    hal_gps_info_t *gps = &p->model.gps_info;

    if (gps->is_valid) {
        p->model.trip_time_sec += SPORT_UPDATE_MS / 1000;

        /* Avg speed from distance / time */
        if (p->model.trip_time_sec > 0) {
            float hours = p->model.trip_time_sec / 3600.0f;
            if (hours > 0.001f)
                p->model.avg_speed = p->model.trip_distance / hours;
        }

        /* Calorie estimate: rough 0.05 cal per km per kg (75kg) */
        p->model.calories = (int)(p->model.trip_distance * 75.0f * 0.05f);
    }

    update_sport_info(p, priv);
}

static void on_map_click(lv_event_t *e)
{
    page_dialplate_t *p = (page_dialplate_t *)lv_event_get_user_data(e);
    pm_push(p->base.manager, "LiveMap", NULL);
}

static void on_menu_click(lv_event_t *e)
{
    page_dialplate_t *p = (page_dialplate_t *)lv_event_get_user_data(e);
    pm_push(p->base.manager, "SystemInfos", NULL);
}

static void on_load(page_t *base)
{
    page_dialplate_t *p = (page_dialplate_t *)base;
    lv_obj_t *root      = base->root;

    /* Allocate private data for sport tracking */
    dialplate_priv_t *priv = (dialplate_priv_t *)malloc(sizeof(dialplate_priv_t));
    memset(priv, 0, sizeof(*priv));
    base->user_data = priv;

    dialplate_view_create(&p->view, root);

    dialplate_model_init(&p->model, g_data_center);

    /* Hook up event callback — fires on every publish */
    p->model.event_cb = on_data_arrived;

    dialplate_model_pull_clock(&p->model);
    dialplate_model_pull_gps(&p->model);
    update_sport_info(p, priv);

    priv->timer = lv_timer_create(on_sport_timer, SPORT_UPDATE_MS, p);

    /* No drag-to-pop here: Dialplate is the bottom page of the stack
     * (startup pm_replace()s into it), so there is nothing to pop back to.
     * pm_pop() would refuse anyway (see pm_state: "only root page remains"). */

    lv_obj_add_event_cb(p->view.btn_cont.btn_map, on_map_click,
                        LV_EVENT_CLICKED, p);
    lv_obj_add_event_cb(p->view.btn_cont.btn_menu, on_menu_click,
                        LV_EVENT_CLICKED, p);
}

static void on_did_unload(page_t *base)
{
    page_dialplate_t *p = (page_dialplate_t *)base;
    dialplate_priv_t *priv = (dialplate_priv_t *)base->user_data;

    if (priv->timer) {
        lv_timer_delete(priv->timer);
        priv->timer = NULL;
    }

    dialplate_model_deinit(&p->model);
    dialplate_view_delete(&p->view);

    free(priv);
    base->user_data = NULL;
}

void page_dialplate_init(page_dialplate_t *p)
{
    page_vtable_t vtable = {
        .on_load       = on_load,
        .on_did_unload = on_did_unload,
    };
    page_init(&p->base, "Dialplate", vtable);

    p->view  = (dialplate_view_t){0};
    p->model = (dialplate_model_t){0};
}

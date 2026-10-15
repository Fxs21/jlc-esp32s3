/**
 * @file startup_page.c
 * @brief Startup page — Presenter (bridges View and Model)
 *
 * Lifecycle (X-Track pattern):
 *   on_load          -> View.Create + Model.Init + 2s one-shot timer
 *   on_will_appear   -> start entry animation timeline
 *   on_did_appear    -> root fade-out (500ms, 1.5s delay)
 *   timer fires      -> pm_replace("Dialplate")
 *   on_did_disappear -> (no-op in simulator)
 *   on_unload        -> timer auto-deleted, View.Delete, Model.Deinit
 */
#include "startup_page.h"
#include "app.h"
#include "log.h"
#include <stdlib.h>

#define TAG "startup"

static void on_timer_cb(lv_timer_t *timer)
{
    page_t *page = (page_t *)lv_timer_get_user_data(timer);
    if (!page || !page->manager)
        return;

    LOG_I(TAG, "Startup timer fired, replacing with Dialplate");
    pm_replace(page->manager, "Dialplate", NULL);
}

static void on_load(page_t *page)
{
    page_startup_t *p = (page_startup_t *)page;

    p->model.account = NULL; /* model_init will set it */
    startup_model_init(&p->model, g_data_center);
    startup_view_create(&p->view, page->root);

    /* One-shot timer: 2s then auto-replace */
    lv_timer_t *timer = lv_timer_create(on_timer_cb, 2000, page);
    lv_timer_set_repeat_count(timer, 1);
}

static void on_will_appear(page_t *page)
{
    page_startup_t *p = (page_startup_t *)page;

    /* Start the entry animation timeline */
    if (p->view.anim_timeline)
        lv_anim_timeline_start(p->view.anim_timeline);
}

static void on_did_appear(page_t *page)
{
    /* Fade out root after 1.5s delay, taking 500ms.
     * This runs in parallel with the 2s one-shot timer.
     * Whichever finishes first is fine — Replace() handles it. */
    lv_obj_fade_out(page->root, 500, 1500);
}

static void on_did_disappear(page_t *page)
{
    page_startup_t *p = (page_startup_t *)page;

    /* Show the status bar (was hidden during startup) */
    startup_model_show_status_bar(&p->model);
}

static void on_unload(page_t *page)
{
    page_startup_t *p = (page_startup_t *)page;

    startup_view_delete(&p->view);
    startup_model_deinit(&p->model);
}

static const page_vtable_t startup_vtable = {
    .on_load        = on_load,
    .on_will_appear = on_will_appear,
    .on_did_appear  = on_did_appear,
    .on_did_disappear = on_did_disappear,
    .on_unload      = on_unload,
};

void page_startup_init(page_startup_t *p, data_center_t *dc)
{
    (void)dc; /* used later in on_load via g_data_center */

    page_init(&p->base, "Startup", startup_vtable);

    /* Match X-Track: no cache, no entry animation */
    page_set_disable_auto_cache(&p->base, true);
    page_set_custom_anim(&p->base, LOAD_ANIM_NONE, 0, NULL);
}

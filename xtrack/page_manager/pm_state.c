/**
 * @file pm_state.c
 * @brief Page lifecycle state machine executors
 *
 * Implements the state machine dispatch table (pm_state_update) and
 * each individual state executor (exec_load through exec_unload).
 * The entering sequence is LOAD -> WILL_APPEAR -> DID_APPEAR -> ACTIVITY.
 * The exiting sequence is ACTIVITY -> WILL_DISAPPEAR -> DID_DISAPPEAR
 * -> (cached ? WILL_APPEAR : UNLOAD) -> IDLE.
 */
#include "pm_internal.h"
#include "log.h"
#include <stdlib.h>

#define TAG "page_manager"

static void exec_load(page_t *page)
{
    LOG_I(TAG, "Page(%s) load", page->name);

    if (page->root != NULL) {
        LOG_W(TAG, "Page(%s) root already exists, reusing", page->name);
        page->priv.state = PAGE_STATE_WILL_APPEAR;
        return;
    }

    page->root = lv_obj_create(lv_scr_act());
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(page->root, page);

    if (page->manager->root_default_style)
        lv_obj_add_style(page->root, page->manager->root_default_style,
                         LV_PART_MAIN);

    if (page->vtable.on_load)
        page->vtable.on_load(page);
    if (page->vtable.on_did_load)
        page->vtable.on_did_load(page);

    /* Cache decision: force_cache > disable_auto_cache > auto-cache */
    if (page->priv.force_cache) {
        page->priv.is_cached = true;
    } else if (page->priv.disable_auto_cache) {
        page->priv.is_cached = false;
    } else {
        page->priv.is_cached = true;
    }

    page->priv.state = PAGE_STATE_WILL_APPEAR;
    pm_state_update(page->manager, page);
}

static void exec_will_appear(page_t *page)
{
    LOG_I(TAG, "Page(%s) will appear", page->name);

    if (page->vtable.on_will_appear)
        page->vtable.on_will_appear(page);

    /* Show root */
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_HIDDEN);

    pm_anim_start(page);
}

static void exec_did_appear(page_t *page)
{
    LOG_I(TAG, "Page(%s) did appear", page->name);

    if (page->vtable.on_did_appear)
        page->vtable.on_did_appear(page);

    page->priv.state = PAGE_STATE_ACTIVITY;
}

static void exec_activity(page_t *page)
{
    LOG_I(TAG, "Page(%s) active break", page->name);
    /* ACTIVITY -> WILL_DISAPPEAR trigger when another page is pushed/popped */
    page->priv.state = PAGE_STATE_WILL_DISAPPEAR;
    pm_state_update(page->manager, page);
}

static void exec_will_disappear(page_t *page)
{
    LOG_I(TAG, "Page(%s) will disappear", page->name);

    if (page->vtable.on_will_disappear)
        page->vtable.on_will_disappear(page);

    pm_anim_start(page);
}

static void exec_did_disappear(page_t *page)
{
    LOG_I(TAG, "Page(%s) did disappear", page->name);

    if (page->vtable.on_did_disappear)
        page->vtable.on_did_disappear(page);

    /* Hide the root */
    lv_obj_add_flag(page->root, LV_OBJ_FLAG_HIDDEN);

    if (page->priv.is_cached) {
        LOG_I(TAG, "Page(%s) cached", page->name);
        page->priv.state = PAGE_STATE_WILL_APPEAR;
    } else {
        page->priv.state = PAGE_STATE_UNLOAD;
        pm_state_update(page->manager, page);
    }
}

static void exec_unload(page_t *page)
{
    LOG_I(TAG, "Page(%s) unload", page->name);

    if (page->vtable.on_unload)
        page->vtable.on_unload(page);

    /* Free stash memory if present */
    if (page->priv.stash.ptr) {
        free(page->priv.stash.ptr);
        page->priv.stash.ptr  = NULL;
        page->priv.stash.size = 0;
    }

    if (page->root) {
        lv_obj_del_async(page->root);
        page->root = NULL;
    }

    page->priv.is_cached = false;

    if (page->vtable.on_did_unload)
        page->vtable.on_did_unload(page);

    page->priv.state = PAGE_STATE_IDLE;
}

void pm_state_update(page_manager_t *pm, page_t *page)
{
    if (!pm || !page)
        return;

    if (page->priv.anim.is_busy)
        return;

    switch (page->priv.state) {
    case PAGE_STATE_IDLE:
        break;

    case PAGE_STATE_LOAD:
        exec_load(page);
        break;

    case PAGE_STATE_WILL_APPEAR:
        exec_will_appear(page);
        break;

    case PAGE_STATE_DID_APPEAR:
        exec_did_appear(page);
        break;

    case PAGE_STATE_ACTIVITY:
        exec_activity(page);
        break;

    case PAGE_STATE_WILL_DISAPPEAR:
        exec_will_disappear(page);
        break;

    case PAGE_STATE_DID_DISAPPEAR:
        exec_did_disappear(page);
        break;

    case PAGE_STATE_UNLOAD:
        exec_unload(page);
        break;

    default:
        LOG_E(TAG, "Page(%s) unknown state %d", page->name, page->priv.state);
        break;
    }
}

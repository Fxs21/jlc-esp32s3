/**
 * @file page_manager.c
 * @brief Page stack lifecycle manager -- route + pool + public API
 *
 * This file manages the page pool (name-to-page registry via vector),
 * implements the public API (push/pop/replace/back_home), and drives
 * the switch transition (pm_switch_to).
 *
 * Animation, state machine execution, and drag-to-pop are in separate
 * files: pm_anim.c, pm_state.c, pm_drag.c.
 */
#include <inttypes.h>
#include "pm_internal.h"
#include "log.h"
#include "uthash/utstack.h"
#include <stdlib.h>
#include <string.h>

#define TAG "page_manager"

static page_t *pm_find_entry(page_manager_t *pm, const char *name)
{
    for (int i = 0; i < pm->page_pool.count; i++) {
        page_t *p = (page_t *)pm->page_pool.items[i];
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

static bool pm_switch_to(page_manager_t *pm, page_t *prev, page_t *cur,
                         bool is_push, const page_stash_t *stash)
{
    if (!pm || !cur)
        return false;

    pm->is_switch_req = true;
    pm->is_entering   = is_push;

    /* Copy stash data to target page (X-Track: allocate + memcpy) */
    if (stash != NULL && stash->ptr != NULL && stash->size > 0) {
        void *buffer = NULL;

        if (cur->priv.stash.ptr == NULL) {
            buffer = malloc(stash->size);
            if (!buffer) {
                LOG_E(TAG, "stash alloc failed (%" PRIu32 " bytes)", stash->size);
                return false;
            }
        } else if (cur->priv.stash.size == stash->size) {
            /* Reuse existing buffer if size matches */
            buffer = cur->priv.stash.ptr;
        } else {
            /* Size mismatch — free and realloc */
            free(cur->priv.stash.ptr);
            cur->priv.stash.ptr = NULL;
            buffer = malloc(stash->size);
            if (!buffer) {
                LOG_E(TAG, "stash realloc failed (%" PRIu32 " bytes)", stash->size);
                return false;
            }
        }

        memcpy(buffer, stash->ptr, stash->size);
        cur->priv.stash.ptr  = buffer;
        cur->priv.stash.size = stash->size;
        LOG_I(TAG, "stash copied (%" PRIu32 " bytes) to Page(%s)", stash->size,
              cur->name);
    }

    /* is_entering: cur = target, prev = departing */
    cur->priv.anim.is_entering = true;
    if (prev)
        prev->priv.anim.is_entering = false;

    /* Determine cur's starting state */
    if (cur->priv.is_cached && cur->root != NULL) {
        LOG_I(TAG, "Page(%s) cached, appear directly", cur->name);
        cur->priv.state = PAGE_STATE_WILL_APPEAR;
    } else if (cur->priv.state == PAGE_STATE_IDLE) {
        cur->priv.state = PAGE_STATE_LOAD;
    } else if (cur->priv.state == PAGE_STATE_WILL_APPEAR) {
        /* Already in appear pipeline (e.g. cached page being restored) */
    } else {
        LOG_W(TAG, "Page(%s) unexpected state %d, forcing LOAD", cur->name,
              cur->priv.state);
        cur->priv.state = PAGE_STATE_LOAD;
    }

    /* Process prev first (departing), then cur (arriving) */
    if (prev) {
        prev->priv.state = PAGE_STATE_ACTIVITY;
        pm_state_update(pm, prev);
    }

    pm_state_update(pm, cur);

    /* Layer order */
    if (is_push) {
        /* New page on top */
        if (prev && prev->root)
            lv_obj_move_foreground(prev->root);
        if (cur->root)
            lv_obj_move_foreground(cur->root);
    } else {
        /* Revealed page underneath, pop page on top (slides out) */
        if (cur->root)
            lv_obj_move_foreground(cur->root);
        if (prev && prev->root)
            lv_obj_move_foreground(prev->root);
    }
    return true;
}

void pm_init(page_manager_t *pm)
{
    if (!pm)
        return;
    memset(pm, 0, sizeof(*pm));
    /* stack = NULL already from memset */
    pm->anim_type = LOAD_ANIM_OVER_LEFT;
    pm->anim_time = 500;
    pm->anim_path = lv_anim_path_ease_out;
    pm->root_default_style = NULL;
}

void pm_install(page_manager_t *pm, page_t *page)
{
    if (!pm || !page || !page->name)
        return;

    const char *name = page->name;

    /* Check for duplicate name / page pointer */
    for (int i = 0; i < pm->page_pool.count; i++) {
        page_t *p = (page_t *)pm->page_pool.items[i];
        if (p == page) {
            LOG_W(TAG, "pm_install: page %p already registered as '%s'",
                  (void *)page, page->name);
            return;
        }
        if (strcmp(p->name, name) == 0) {
            LOG_W(TAG, "pm_install: name '%s' already registered (page %p)",
                  name, (void *)p);
            return;
        }
    }

    if (vector_push(&pm->page_pool, page) != 0) {
        LOG_E(TAG, "pm_install: vector_push failed");
        return;
    }

    /* Set the name on the page so it's available during lifecycle */
    page->name    = name;
    page->manager = pm;

    LOG_I(TAG, "Page(%s) installed", name);
}

void pm_set_global_anim(page_manager_t *pm, load_anim_t anim, uint16_t time,
                        lv_anim_path_cb_t path)
{
    if (!pm)
        return;
    if (anim >= LOAD_ANIM_LAST)
        anim = LOAD_ANIM_NONE;
    pm->anim_type = anim;
    pm->anim_time = time;
    pm->anim_path = path;
}

void pm_set_root_default_style(page_manager_t *pm, lv_style_t *style)
{
    if (!pm)
        return;
    pm->root_default_style = style;
}

page_t *pm_top(page_manager_t *pm)
{
    if (!pm || !pm->stack)
        return NULL;
    return STACK_TOP(pm->stack);
}

int pm_depth(page_manager_t *pm)
{
    if (!pm || !pm->stack)
        return 0;
    page_t *tmp;
    int count;
    STACK_COUNT(pm->stack, tmp, count);
    return count;
}

bool pm_push(page_manager_t *pm, const char *name,
             const page_stash_t *stash)
{
    if (!pm || !name)
        return false;

    page_t *page = pm_find_entry(pm, name);
    if (!page) {
        LOG_E(TAG, "Page(%s) not installed", name);
        return false;
    }

    if (pm->is_switch_req || pm->busy_count > 0) {
        LOG_W(TAG, "Page(%s) push: switch busy, ignored", name);
        return false;
    }

    /* Check for duplicate */
    for (page_t *iter = pm->stack; iter; iter = iter->next) {
        if (iter == page) {
            LOG_W(TAG, "Page(%s) already on stack", name);
            return false;
        }
    }

    page_t *prev = STACK_TOP(pm->stack);

    STACK_PUSH(pm->stack, page);

    LOG_I(TAG, "Page(%s) push >> (stash=%s)", name,
          stash ? "yes" : "no");

    return pm_switch_to(pm, prev, page, true, stash);
}

bool pm_pop(page_manager_t *pm)
{
    if (!pm || !pm->stack)
        return false;

    if (pm->is_switch_req || pm->busy_count > 0) {
        LOG_W(TAG, "pop: switch busy, ignored");
        return false;
    }

    /* Can't pop the last page */
    if (pm->stack->next == NULL) {
        LOG_W(TAG, "pop: only root page remains");
        return false;
    }

    page_t *old;
    STACK_POP(pm->stack, old);

    /* If using default auto-cache, don't cache the popped page */
    if (!old->priv.disable_auto_cache)
        old->priv.is_cached = false;

    page_t *cur = STACK_TOP(pm->stack);

    LOG_I(TAG, "Page(%s) pop <<", old->name);

    pm_switch_to(pm, old, cur, false, NULL);
    return true;
}

bool pm_replace(page_manager_t *pm, const char *name,
                const page_stash_t *stash)
{
    if (!pm || !name)
        return false;

    page_t *page = pm_find_entry(pm, name);
    if (!page) {
        LOG_E(TAG, "Page(%s) not installed", name);
        return false;
    }

    for (page_t *iter = pm->stack; iter; iter = iter->next) {
        if (iter == page) {
            LOG_E(TAG, "Page(%s) already on stack, replace rejected", name);
            return false;
        }
    }

    if (pm->is_switch_req || pm->busy_count > 0) {
        LOG_W(TAG, "Page(%s) replace: switch busy, ignored", name);
        return false;
    }

    page_t *old = STACK_TOP(pm->stack);

    if (old) {
        old->priv.is_cached          = false;
        old->priv.disable_auto_cache = true;
        STACK_POP(pm->stack, old);
    }

    STACK_PUSH(pm->stack, page);

    LOG_I(TAG, "Page(%s) replace (stash=%s)", name,
          stash ? "yes" : "no");

    return pm_switch_to(pm, old, page, true, stash);
}

bool pm_back_home(page_manager_t *pm)
{
    if (!pm || !pm->stack)
        return false;

    if (pm->is_switch_req || pm->busy_count > 0) {
        LOG_W(TAG, "back_home: switch busy, ignored");
        return false;
    }

    if (pm->stack->next == NULL) {
        LOG_W(TAG, "back_home: already at root");
        return false;
    }

    /* Pop all pages above root and force-unload them */
    while (pm->stack != NULL && pm->stack->next != NULL) {
        page_t *p;
        STACK_POP(pm->stack, p);

        /* Run forced lifecycle (synchronous, no animation) */
        if (p->priv.state == PAGE_STATE_ACTIVITY) {
            if (p->vtable.on_will_disappear)
                p->vtable.on_will_disappear(p);
            if (p->vtable.on_did_disappear)
                p->vtable.on_did_disappear(p);
        }

        if (p->vtable.on_unload)
            p->vtable.on_unload(p);

        /* Free stash memory if present */
        if (p->priv.stash.ptr) {
            free(p->priv.stash.ptr);
            p->priv.stash.ptr = NULL;
            p->priv.stash.size = 0;
        }

        if (p->root) {
            lv_obj_del_async(p->root);
            p->root = NULL;
        }
        p->priv.is_cached = false;
        if (p->vtable.on_did_unload)
            p->vtable.on_did_unload(p);
        p->priv.state = PAGE_STATE_IDLE;
    }

    page_t *home = STACK_TOP(pm->stack);

    LOG_I(TAG, "back_home -> Page(%s)", home->name);

    pm_switch_to(pm, NULL, home, false, NULL);
    return true;
}

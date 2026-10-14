/**
 * @file pm_anim.c
 * @brief Animation parameter resolution, attribute table, and animation starter
 *
 * Provides per-page animation parameter resolution (fallback chain:
 * page override -> global default -> hard default).  The animation
 * attribute table (pm_get_anim_attr) maps each load_anim_t to concrete
 * start/end positions for push/pop enter/exit roles.
 *
 * pm_anim_start is the unified entry point for both entering and exiting
 * page animations, called from pm_state.c exec_will_appear/exec_will_disappear.
 */
#include "pm_internal.h"
#include "log.h"
#include <string.h>

#define TAG "page_manager"

load_anim_t page_anim_type(page_t *page)
{
    if (page->priv.anim.override.active)
        return page->priv.anim.override.anim_type;
    return page->manager->anim_type;
}

uint16_t page_anim_time(page_t *page)
{
    if (page->priv.anim.override.active &&
        page->priv.anim.override.anim_time > 0)
        return page->priv.anim.override.anim_time;
    return page->manager->anim_time ? page->manager->anim_time : 300;
}

lv_anim_path_cb_t page_anim_path(page_t *page)
{
    if (page->priv.anim.override.active &&
        page->priv.anim.override.anim_path != NULL)
        return page->priv.anim.override.anim_path;
    return page->manager->anim_path ? page->manager->anim_path
                                    : lv_anim_path_ease_out;
}

typedef struct pm_anim_attr_t {
    bool is_slide; /* false = opacity animation */
    bool is_hor;
    bool is_vert;
    int32_t push_enter_start, push_enter_end;
    int32_t push_exit_start, push_exit_end;
    int32_t pop_enter_start, pop_enter_end;
    int32_t pop_exit_start, pop_exit_end;
} pm_anim_attr_t;

/* Scale symbolic values by screen dimensions */
#define HOR(w)     (w)
#define HOR_NEG(w) (-(int32_t)(w))
#define VER(h)     (h)
#define VER_NEG(h) (-(int32_t)(h))
#define OPA_T      ((int32_t)LV_OPA_TRANSP)
#define OPA_C      ((int32_t)LV_OPA_COVER)

static bool pm_get_anim_attr(load_anim_t type, lv_coord_t hor, lv_coord_t ver,
                             pm_anim_attr_t *out_attr)
{
    memset(out_attr, 0, sizeof(*out_attr));

    switch (type) {
    case LOAD_ANIM_OVER_LEFT:
        out_attr->is_slide         = true;
        out_attr->is_hor           = true;
        out_attr->push_enter_start = HOR(hor);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = 0;
        out_attr->pop_enter_start  = 0;
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = HOR(hor);
        return true;

    case LOAD_ANIM_OVER_RIGHT:
        out_attr->is_slide         = true;
        out_attr->is_hor           = true;
        out_attr->push_enter_start = HOR_NEG(hor);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = 0;
        out_attr->pop_enter_start  = 0;
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = HOR_NEG(hor);
        return true;

    case LOAD_ANIM_OVER_TOP:
        out_attr->is_slide         = true;
        out_attr->is_vert          = true;
        out_attr->push_enter_start = VER(ver);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = 0;
        out_attr->pop_enter_start  = 0;
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = VER(ver);
        return true;

    case LOAD_ANIM_OVER_BOTTOM:
        out_attr->is_slide         = true;
        out_attr->is_vert          = true;
        out_attr->push_enter_start = VER_NEG(ver);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = 0;
        out_attr->pop_enter_start  = 0;
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = VER_NEG(ver);
        return true;

    case LOAD_ANIM_MOVE_LEFT:
        out_attr->is_slide         = true;
        out_attr->is_hor           = true;
        out_attr->push_enter_start = HOR(hor);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = HOR_NEG(hor);
        out_attr->pop_enter_start  = HOR_NEG(hor);
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = HOR(hor);
        return true;

    case LOAD_ANIM_MOVE_RIGHT:
        out_attr->is_slide         = true;
        out_attr->is_hor           = true;
        out_attr->push_enter_start = HOR_NEG(hor);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = HOR(hor);
        out_attr->pop_enter_start  = HOR(hor);
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = HOR_NEG(hor);
        return true;

    case LOAD_ANIM_MOVE_TOP:
        out_attr->is_slide         = true;
        out_attr->is_vert          = true;
        out_attr->push_enter_start = VER(ver);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = VER_NEG(ver);
        out_attr->pop_enter_start  = VER_NEG(ver);
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = VER(ver);
        return true;

    case LOAD_ANIM_MOVE_BOTTOM:
        out_attr->is_slide         = true;
        out_attr->is_vert          = true;
        out_attr->push_enter_start = VER_NEG(ver);
        out_attr->push_enter_end   = 0;
        out_attr->push_exit_start  = 0;
        out_attr->push_exit_end    = VER(ver);
        out_attr->pop_enter_start  = VER(ver);
        out_attr->pop_enter_end    = 0;
        out_attr->pop_exit_start   = 0;
        out_attr->pop_exit_end     = VER_NEG(ver);
        return true;

    case LOAD_ANIM_FADE_ON:
        out_attr->is_slide         = false;
        out_attr->push_enter_start = OPA_T;
        out_attr->push_enter_end   = OPA_C;
        out_attr->push_exit_start  = OPA_C;
        out_attr->push_exit_end    = OPA_C;
        out_attr->pop_enter_start  = OPA_C;
        out_attr->pop_enter_end    = OPA_C;
        out_attr->pop_exit_start   = OPA_C;
        out_attr->pop_exit_end     = OPA_T;
        return true;

    case LOAD_ANIM_FADE:
        out_attr->is_slide         = false;
        out_attr->push_enter_start = OPA_T;
        out_attr->push_enter_end   = OPA_C;
        out_attr->push_exit_start  = OPA_C;
        out_attr->push_exit_end    = OPA_T;
        out_attr->pop_enter_start  = OPA_C;
        out_attr->pop_enter_end    = OPA_T;
        out_attr->pop_exit_start   = OPA_T;
        out_attr->pop_exit_end     = OPA_C;
        return true;

    case LOAD_ANIM_NONE:
        return true; /* all zeros = no animation */

    default:
        LOG_E(TAG, "Unknown animation type %d", type);
        return false;
    }
}

static void on_anim_finish(lv_anim_t *a)
{
    page_t *page       = (page_t *)lv_anim_get_user_data(a);
    page_manager_t *pm = page->manager;

    LOG_I(TAG, "Page(%s) anim done", page->name);

    page->priv.anim.is_busy = false;
    if (pm->busy_count > 0)
        pm->busy_count--;

    /* Advance to the appropriate next state */
    if (page->priv.anim.is_entering) {
        page->priv.state = PAGE_STATE_DID_APPEAR;
    } else {
        page->priv.state = PAGE_STATE_DID_DISAPPEAR;
    }
    pm_state_update(pm, page);

    /* Check if the whole switch is finished */
    if (pm->busy_count == 0) {
        pm->is_switch_req = false;
        LOG_I(TAG, "---- Page switch finished ----");
    }
}

void pm_anim_start(page_t *page)
{
    page_manager_t *pm = page->manager;
    load_anim_t atype  = page_anim_type(page);

    if (atype == LOAD_ANIM_NONE) {
        /* No animation -- proceed directly */
        if (page->priv.anim.is_entering) {
            page->priv.state = PAGE_STATE_DID_APPEAR;
        } else {
            page->priv.state = PAGE_STATE_DID_DISAPPEAR;
        }
        pm_state_update(pm, page);
        pm->is_switch_req = false;
        LOG_I(TAG, "---- Page switch finished (no anim) ----");
        return;
    }

    lv_display_t *disp = lv_display_get_default();
    lv_coord_t w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t h = lv_display_get_vertical_resolution(disp);

    pm_anim_attr_t attr;
    if (!pm_get_anim_attr(atype, w, h, &attr)) {
        /* Unknown type -- skip anim */
        if (page->priv.anim.is_entering) {
            page->priv.state = PAGE_STATE_DID_APPEAR;
        } else {
            page->priv.state = PAGE_STATE_DID_DISAPPEAR;
        }
        pm_state_update(pm, page);
        pm->is_switch_req = false;
        LOG_W(TAG, "Unknown anim type %d, switch cleared", atype);
        return;
    }

    /* Determine which value pair to use based on push/pop and entering/exiting */
    bool is_push     = pm->is_entering;
    bool page_enters = page->priv.anim.is_entering;

    int32_t start_val, end_val;

    if (is_push) {
        if (page_enters) {
            start_val = attr.push_enter_start;
            end_val   = attr.push_enter_end;
        } else {
            start_val = attr.push_exit_start;
            end_val   = attr.push_exit_end;
        }
    } else {
        if (page_enters) {
            start_val = attr.pop_enter_start;
            end_val   = attr.pop_enter_end;
        } else {
            start_val = attr.pop_exit_start;
            end_val   = attr.pop_exit_end;
        }
    }

    /* If this is a pop-exit triggered by drag, start from the drag
     * position instead of snapping back to the animation's default start. */
    lv_coord_t orig_start = start_val;
    lv_coord_t drag_start = page->priv.drag.exit_pos;
    if (!page_enters && !is_push && drag_start != 0) {
        page->priv.drag.exit_pos = 0; /* consume */
        start_val = drag_start;
    }

    if (start_val == end_val) {
        /* No visual change -- skip LVGL animation, advance state */
        if (page_enters) {
            page->priv.state = PAGE_STATE_DID_APPEAR;
        } else {
            page->priv.state = PAGE_STATE_DID_DISAPPEAR;
        }
        pm_state_update(pm, page);
        return;
    }

    /* Actually animate */
    lv_anim_exec_xcb_t setter;
    if (attr.is_slide) {
        setter = (lv_anim_exec_xcb_t)(attr.is_hor ? lv_obj_set_x : lv_obj_set_y);
    } else {
        setter = (lv_anim_exec_xcb_t)lv_obj_set_style_opa;
    }

    /* Set initial position */
    setter(page->root, start_val);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, page->root);
    lv_anim_set_exec_cb(&a, setter);
    lv_anim_set_values(&a, start_val, end_val);

    uint16_t full_time = page_anim_time(page);
    int32_t  full_dist = end_val - orig_start;
    int32_t  rem_dist  = end_val - start_val;
    if (full_dist != 0) {
        uint32_t full_abs = (uint32_t)(full_dist < 0 ? -full_dist : full_dist);
        uint32_t rem_abs  = (uint32_t)(rem_dist  < 0 ? -rem_dist  : rem_dist);
        if (rem_abs < full_abs)
            full_time = (uint16_t)((int64_t)full_time * rem_abs / full_abs);
    }
    lv_anim_set_time(&a, full_time);

    lv_anim_set_path_cb(&a, page_anim_path(page));
    lv_anim_set_ready_cb(&a, on_anim_finish);
    lv_anim_set_user_data(&a, page);
    /* Mark busy BEFORE starting the animation.  If LVGL fires the
     * ready callback synchronously, the callback will decrement
     * busy_count and we must not re-increment it afterwards. */
    page->priv.anim.is_busy = true;
    pm->busy_count++;

    lv_anim_start(&a);
}

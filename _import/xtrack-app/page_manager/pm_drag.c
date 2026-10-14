/**
 * @file pm_drag.c
 * @brief Gesture-to-pop (drag-back) support for pages
 *
 * Implements LVGL event callbacks that allow users to drag a page
 * sideways/upwards to trigger a pop operation.  The drag direction
 * is determined by the page's effective animation type.
 *
 * When released beyond 50% of the exit distance (with inertia
 * prediction), the drag handler calls pm_pop() directly.
 */
#include "pm_internal.h"
#include "log.h"
#include <stdlib.h>

#define TAG "page_manager"

/* Friction percentage for inertia prediction (20 = 80% retained per step) */
#define PM_DRAG_THROW 20

/* Drag axis detection */
typedef enum {
    DRAG_NONE,
    DRAG_HOR,
    DRAG_VER
} drag_axis_t;

static drag_axis_t pm_get_drag_axis(load_anim_t type)
{
    switch (type) {
    case LOAD_ANIM_OVER_LEFT:
    case LOAD_ANIM_OVER_RIGHT:
    case LOAD_ANIM_MOVE_LEFT:
    case LOAD_ANIM_MOVE_RIGHT:
        return DRAG_HOR;

    case LOAD_ANIM_OVER_TOP:
    case LOAD_ANIM_OVER_BOTTOM:
    case LOAD_ANIM_MOVE_TOP:
    case LOAD_ANIM_MOVE_BOTTOM:
        return DRAG_VER;

    default:
        return DRAG_NONE;
    }
}

/* Position helpers (no static variables, operate on page->priv.drag) */

static lv_coord_t drag_get_pos(page_t *page)
{
    load_anim_t atype = page_anim_type(page);
    return (pm_get_drag_axis(atype) == DRAG_HOR)
               ? lv_obj_get_x(page->root)
               : lv_obj_get_y(page->root);
}

static void drag_set_pos(page_t *page, lv_coord_t v)
{
    load_anim_t atype = page_anim_type(page);
    if (pm_get_drag_axis(atype) == DRAG_HOR)
        lv_obj_set_x(page->root, v);
    else
        lv_obj_set_y(page->root, v);
}

/* Wrapper for drag snap-back animations.  Distinct function pointer prevents
 * lv_anim_del from accidentally matching PM animations that use the same
 * underlying setter (lv_obj_set_x / lv_obj_set_y).  Retrieves the page via
 * lv_obj_get_user_data (set by exec_load). */
static void drag_anim_set_pos(lv_obj_t *obj, lv_coord_t v)
{
    page_t *page = (page_t *)lv_obj_get_user_data(obj);
    drag_set_pos(page, v);
}

static void on_root_drag_anim_finish(lv_anim_t *a)
{
    page_t *page            = (page_t *)lv_anim_get_user_data(a);
    page_manager_t *pm      = page->manager;
    page->priv.anim.is_busy = false;
    pm->busy_count--;

    /* Hide bottom page again after snap-back */
    if (pm->stack != NULL && pm->stack->next != NULL) {
        page_t *bottom = pm->stack->next;
        if (bottom && bottom->root) {
            lv_obj_add_flag(bottom->root, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* Compute drag bounds from the page's effective animation type.
   Returns true if drag is supported for this axis. */
static bool compute_drag_bounds(page_t *page, int32_t *out_min,
                                int32_t *out_max, int32_t *out_exit_off)
{
    lv_display_t *disp = lv_display_get_default();

    load_anim_t atype = page_anim_type(page);
    drag_axis_t axis  = pm_get_drag_axis(atype);
    if (axis == DRAG_NONE) {
        *out_min = *out_max = *out_exit_off = 0;
        return false;
    }

    lv_coord_t dim;
    if (axis == DRAG_HOR) {
        dim = lv_display_get_horizontal_resolution(disp);
    } else {
        dim = lv_display_get_vertical_resolution(disp);
    }
    if (dim <= 0) {
        *out_min = *out_max = *out_exit_off = 0;
        return false;
    }

    switch (atype) {
    case LOAD_ANIM_OVER_LEFT:
    case LOAD_ANIM_MOVE_LEFT:
        *out_min      = 0;
        *out_max      = dim;
        *out_exit_off = dim;
        return true;

    case LOAD_ANIM_OVER_RIGHT:
    case LOAD_ANIM_MOVE_RIGHT:
        *out_min      = -dim;
        *out_max      = 0;
        *out_exit_off = dim;
        return true;

    case LOAD_ANIM_OVER_TOP:
    case LOAD_ANIM_MOVE_TOP:
        *out_min      = 0;
        *out_max      = dim;
        *out_exit_off = dim;
        return true;

    case LOAD_ANIM_OVER_BOTTOM:
    case LOAD_ANIM_MOVE_BOTTOM:
        *out_min      = -dim;
        *out_max      = 0;
        *out_exit_off = dim;
        return true;

    default:
        *out_min = *out_max = *out_exit_off = 0;
        return false;
    }
}

static void on_root_drag_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *root       = lv_event_get_current_target(e);
    page_t *page         = (page_t *)lv_event_get_user_data(e);
    page_manager_t *pm   = page->manager;

    if (pm == NULL || !page->priv.drag.is_enabled)
        return;

    /* If the user was actually dragging (not a tap), consume the
     * following CLICKED event so it doesn't reach the page's own
     * click handler (e.g. on_info_click -> pm_push). */
    if (code == LV_EVENT_CLICKED) {
        if (page->priv.drag.is_dragged) {
            page->priv.drag.is_dragged = false;
            lv_event_stop_processing(e);
        }
        return;
    }

    /* Only handle user gesture events */
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
        code != LV_EVENT_RELEASED)
        return;

    int32_t min_pos, max_pos, exit_offset;
    if (!compute_drag_bounds(page, &min_pos, &max_pos, &exit_offset))
        return;

    drag_axis_t axis = pm_get_drag_axis(page_anim_type(page));

    if (code == LV_EVENT_PRESSED) {
        /* Record press coordinate and current page offset */
        lv_indev_get_point(lv_indev_get_act(), &page->priv.drag.press_pos);
        page->priv.drag.page_pos = drag_get_pos(page);

        /* Reset: we don't know yet if this press will turn into a drag */
        page->priv.drag.is_dragged = false;

        /* Interrupt a running snap-back animation.
         * Only clean up busy_count if we actually deleted something.
         * (Returns 0 when no drag animation existed, e.g. PM is running.) */
        if (lv_anim_del(root, (lv_anim_exec_xcb_t)drag_anim_set_pos) > 0) {
            page->priv.anim.is_busy = false;
            if (pm->busy_count > 0)
                pm->busy_count--;
        }

        /* Reveal the page underneath while dragging */
        if (pm->stack != NULL && pm->stack->next != NULL) {
            page_t *bottom = pm->stack->next;
            if (bottom && bottom->root) {
                lv_obj_clear_flag(bottom->root, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else if (code == LV_EVENT_PRESSING) {
        if (exit_offset == 0)
            return;

        lv_point_t current_pos;
        lv_indev_get_point(lv_indev_get_act(), &current_pos);
        lv_coord_t offset = (axis == DRAG_HOR)
                                ? (current_pos.x - page->priv.drag.press_pos.x)
                                : (current_pos.y - page->priv.drag.press_pos.y);
        lv_coord_t target = page->priv.drag.page_pos + offset;
        lv_coord_t clamped = LV_CLAMP(min_pos, target, max_pos);
        drag_set_pos(page, clamped);
    } else if (code == LV_EVENT_RELEASED) {
        if (exit_offset == 0)
            return;

        lv_coord_t start_pos = drag_get_pos(page);

        /* If already at the rest position (no actual drag), skip
         * snap-back -- otherwise busy_count blocks any immediate push. */
        if (start_pos == 0) {
            return;
        }

        /* The user dragged -- suppress the CLICKED event that follows. */
        page->priv.drag.is_dragged = true;

        /* Compute inertia prediction */
        lv_point_t vect;
        lv_indev_get_vect(lv_indev_get_act(), &vect);
        lv_coord_t predict = 0;
        lv_coord_t v       = (axis == DRAG_HOR) ? vect.x : vect.y;
        while (v != 0) {
            predict += v;
            v = v * (100 - PM_DRAG_THROW) / 100;
            if (abs(predict) >= exit_offset)
                break;
        }

        lv_coord_t predicted_end = start_pos + predict;

        /* If predicted end exceeds 50% of the exit distance,
         * and there is a page underneath to pop to, trigger pop. */
        if (abs(predicted_end) > (exit_offset / 2) &&
            pm->stack != NULL && pm->stack->next != NULL) {
            LOG_I(TAG, "Page(%s) drag-pop triggered (pos=%d, pred=%d, thresh=%d)",
                  page->name, (int)start_pos, (int)predicted_end,
                  (int)(exit_offset / 2));
            page->priv.drag.exit_pos = start_pos;
            page->priv.drag.is_enabled = false; /* stop drag; PM animation takes over */
            pm_pop(pm);
            return;
        }

        /* Snap back to rest position */
        page->priv.anim.is_busy = true;
        pm->busy_count++;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, root);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)drag_anim_set_pos);
        lv_anim_set_values(&a, start_pos, 0);

        uint16_t snap_time = page_anim_time(page);
        if (exit_offset > 0)
            snap_time = (uint16_t)((int64_t)snap_time * start_pos / exit_offset);
        lv_anim_set_time(&a, snap_time);

        lv_anim_set_path_cb(&a, page_anim_path(page));
        lv_anim_set_ready_cb(&a, on_root_drag_anim_finish);
        lv_anim_set_user_data(&a, page);
        lv_anim_start(&a);
    }
}

void pm_root_enable_drag(page_manager_t *pm, page_t *page)
{
    if (!pm || !page || !page->root)
        return;

    page->priv.drag.is_enabled = true;
    lv_obj_add_event_cb(page->root, on_root_drag_event, LV_EVENT_ALL, page);
    LOG_I(TAG, "Page(%s) root drag enabled", page->name);
}

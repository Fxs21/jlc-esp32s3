/**
 * @file  page.h
 * @brief Page base with state-machine-driven lifecycle
 *
 * State machine (driven by PageManager):
 *
 *   IDLE -> LOAD -> WILL_APPEAR -> DID_APPEAR -> ACTIVITY
 *                                                  |
 *                                                  v
 *                                            WILL_DISAPPEAR
 *                                                  |
 *                                                  v
 *                                            DID_DISAPPEAR
 *                                             /         \
 *                                        cached     not cached
 *                                           |            |
 *                                           v            v
 *                                     WILL_APPEAR   UNLOAD -> IDLE
 *
 * Each page owns an LVGL root object (child of lv_scr_act()).
 * PageManager creates/destroys the root and drives state transitions.
 */
#ifndef PAGE_H
#define PAGE_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct page_manager_t page_manager_t;
typedef struct page_t page_t;

/**
 * @brief  Stash descriptor for page-to-page parameter passing
 * @note   Used with pm_push() / pm_replace().
 *         The caller provides data+size; the PM copies it internally.
 *         The target page reads it via page_stash_pop() in on_load.
 */
typedef struct {
    void *ptr;
    uint32_t size;
} page_stash_t;

/**
 * @brief  Create a stash literal from a stack variable
 * @param  data: Variable to stash (must outlive the push call)
 * @retval page_stash_t suitable for passing to pm_push()
 * @note   Usage: int type = SPORT_RUNNING;
 *         pm_push(pm, "Run", PAGE_STASH_MAKE(type));
 */
#define page_stash_make(data) ((page_stash_t){&(data), sizeof(data)})

typedef enum {
    PAGE_STATE_IDLE,           /**< Not loaded / terminal */
    PAGE_STATE_LOAD,           /**< Enter: load root & widgets */
    PAGE_STATE_WILL_APPEAR,    /**< Enter: about to become visible */
    PAGE_STATE_DID_APPEAR,     /**< Enter: just became visible */
    PAGE_STATE_ACTIVITY,       /**< Active / running */
    PAGE_STATE_WILL_DISAPPEAR, /**< Exit: about to become invisible */
    PAGE_STATE_DID_DISAPPEAR,  /**< Exit: just became invisible */
    PAGE_STATE_UNLOAD,         /**< Exit: destroy root & widgets */
    PAGE_STATE_LAST
} page_state_t;

typedef enum {
    LOAD_ANIM_NONE,        /* Instant switch */
    LOAD_ANIM_OVER_LEFT,   /* New page slides in from right, covers old */
    LOAD_ANIM_OVER_RIGHT,  /* New page slides in from left, covers old */
    LOAD_ANIM_OVER_TOP,    /* New page slides in from bottom, covers old */
    LOAD_ANIM_OVER_BOTTOM, /* New page slides in from top, covers old */
    LOAD_ANIM_MOVE_LEFT,   /* New page from right, old page pushed left */
    LOAD_ANIM_MOVE_RIGHT,  /* New page from left, old page pushed right */
    LOAD_ANIM_MOVE_TOP,    /* New page from bottom, old page pushed up */
    LOAD_ANIM_MOVE_BOTTOM, /* New page from top, old page pushed down */
    LOAD_ANIM_FADE_ON,     /* New page fades in (old page stays) */
    LOAD_ANIM_FADE,        /* Cross-fade */
    LOAD_ANIM_LAST
} load_anim_t;

typedef struct page_vtable_t {
    /** Create widgets on page->root. Called once per load cycle. */
    void (*on_load)(page_t *page);
    /** Widgets created, root fully ready. */
    void (*on_did_load)(page_t *page);
    /** About to become visible (animation start). */
    void (*on_will_appear)(page_t *page);
    /** Just became visible (animation end). */
    void (*on_did_appear)(page_t *page);
    /** About to become invisible (animation start). */
    void (*on_will_disappear)(page_t *page);
    /** Just became invisible (animation end). */
    void (*on_did_disappear)(page_t *page);
    /** Destroy widgets before unload. */
    void (*on_unload)(page_t *page);
    /** Unload complete; page->root is NULL (LVGL obj queued for async del). */
    void (*on_did_unload)(page_t *page);
} page_vtable_t;

/*
 * Page struct (public, but priv is PM-internal)
 * Public fields (name/root/manager/user_data/vtable) may be read
 * by application code.  The priv section is managed exclusively by
 * the PageManager -- application code MUST use the setter functions
 * (page_set_force_cache, page_set_disable_auto_cache,
 * page_set_custom_anim, page_clear_custom_anim) instead of
 * writing priv directly.
 */

struct page_t {
    const char *name;        /**< Page name (static string) */
    lv_obj_t *root;          /**< UI root child of lv_scr_act() */
    page_manager_t *manager; /**< Owning PageManager */
    struct page_t *next;     /**< Linked-list link for PageManager stack */
    void *user_data;         /**< Free for app use */

    page_vtable_t vtable; /**< Lifecycle hooks */

    struct {
        page_state_t state; /**< Current state machine state */
        bool is_cached;     /**< Root kept alive when not visible */
        bool force_cache;   /**< Always cache (overrides disable_auto_cache) */
        bool disable_auto_cache; /**< Opt out of auto-cache */

        page_stash_t stash; /**< Incoming parameter from pm_push */

        /** Per-page drag-to-pop state */
        struct {
            bool is_enabled;      /**< Gesture-to-pop enabled */
            bool is_dragged;      /**< True when this press turned into a drag */
            lv_point_t press_pos; /**< Pointer screen-coordinate at PRESSED */
            lv_coord_t page_pos;  /**< Page offset at PRESSED (X for HOR drag, Y for VER drag) */
            lv_coord_t exit_pos;  /**< Page offset when drag handed over to pop */
        } drag;

        /** Per-page animation state (overrides PM global) */
        struct {
            bool is_busy;     /**< This page's animation is playing */
            bool is_entering; /**< This page is the entering party */

            /** Per-page override. active=true -> use these values;
                active=false -> use PM global defaults. */
            struct {
                bool active;
                load_anim_t anim_type;
                uint16_t anim_time;
                lv_anim_path_cb_t anim_path;
            } override;
        } anim;
    } priv;
};

/**
 * @brief  Initialize a page struct
 * @param  page:   Pointer to the page instance
 * @param  name:   Page name (static string, not copied)
 * @param  vtable: Lifecycle hooks (may be partially NULL)
 * @retval None
 */
void page_init(page_t *page, const char *name, page_vtable_t vtable);

/**
 * @brief  Override animation for this specific page
 * @param  page:  Pointer to the page instance
 * @param  type:  Animation type (LOAD_ANIM_OVER_LEFT, LOAD_ANIM_NONE, etc.)
 * @param  time:  Duration in ms
 * @param  path:  Animation curve callback
 * @retval None
 * @note   When set, this page ignores the PageManager's global animation.
 *         Call page_clear_custom_anim() to revert to global.
 */
void page_set_custom_anim(page_t *page, load_anim_t type, uint16_t time,
                          lv_anim_path_cb_t path);

/**
 * @brief  Clear per-page animation override, revert to PM global
 * @param  page: Pointer to the page instance
 * @retval None
 */
void page_clear_custom_anim(page_t *page);

/**
 * @brief  Override cache policy: force this page to be cached
 * @param  page:   Pointer to the page instance
 * @param  enable: true = force cache, false = allow auto-cache decision
 * @retval None
 * @note   Overrides disable_auto_cache. Use when auto-cache is globally
 *         disabled but this page still needs caching.
 */
void page_set_force_cache(page_t *page, bool enable);

/**
 * @brief  Opt out of automatic cache management
 * @param  page:    Pointer to the page instance
 * @param  disable: true = disable auto-cache (page will unload on pop)
 * @retval None
 * @note   If force_cache was also set, force_cache takes priority.
 */
void page_set_disable_auto_cache(page_t *page, bool disable);

/**
 * @brief  Pop (read and free) the stash data in on_load
 * @param  page: Pointer to the page instance
 * @param  ptr:  Buffer to receive the data
 * @param  size: Size of the buffer in bytes
 * @retval true on success, false if no stash or size mismatch
 * @note   Must be called from on_load or on_did_load.
 *         The stash memory is freed after copying.
 *         Size must match exactly (X-Track convention).
 */
bool page_stash_pop(page_t *page, void *ptr, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* PAGE_H */

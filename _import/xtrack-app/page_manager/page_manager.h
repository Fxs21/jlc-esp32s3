/**
 * @file  page_manager.h
 * @brief Stack-based page lifecycle manager with state machine
 *
 * Manages a stack of pages and drives the state machine:
 *
 *   pm_push(name):
 *     load -> will_appear -> (anim) -> did_appear -> activity
 *
 *   pm_pop():
 *     old: will_disappear -> (anim) -> did_disappear -> (cached? wait : unload)
 *     new: will_appear -> did_appear -> activity
 *
 * Pages are registered via pm_install() with a unique name, and
 * referenced by name in pm_push()/pm_replace().
 */
#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "page.h"
#include "vector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Page manager structure
 *
 * The stack is a singly-linked list via page->next, managed by
 * the utstack macros (STACK_PUSH/STACK_POP/STACK_TOP).  head=NULL
 * means empty stack, head is the top of stack.
 *
 * page_pool is a vector of page_t pointers mapping
 * names to page_t pointers (names come from page->name).
 *
 * Public fields (anim_type/anim_time/anim_path) may be read by
 * application code.  Internal fields (stack/page_pool/
 * is_switch_req/busy_count/is_entering) are managed exclusively
 * by the PageManager.
 */
typedef struct page_manager_t {
    page_t *stack;      /**< Page stack (linked list via page->next, NULL=empty) */
    vector_t page_pool; /**< Name-to-page registry (vector of page_t*) */

    /* Global animation defaults */
    load_anim_t anim_type;       /**< Default animation for all switches */
    uint16_t anim_time;          /**< Animation duration (ms) */
    lv_anim_path_cb_t anim_path; /**< Animation curve */

    /* PM internal state -- application code should not touch these */
    bool is_switch_req; /**< Switch request pending (debounce) */
    uint8_t busy_count; /**< Number of pages currently animating */
    bool is_entering;   /**< Current switch is push(true) / pop(false) */

    lv_style_t *root_default_style; /**< Optional style applied to every page root */
} page_manager_t;

/**
 * @brief  Initialize the page manager (stack empty, pool empty)
 * @param  pm: Pointer to the page manager instance
 * @retval None
 */
void pm_init(page_manager_t *pm);

/**
 * @brief  Register a page in the pool by name
 * @param  pm:   Pointer to the page manager
 * @param  page: Pointer to the page instance (page->name must be set)
 * @retval None
 * @note   Reads the page name from page->name and registers it.
 *         Also sets page->manager = pm.
 *         Rejects duplicate names or duplicate page pointers.
 */
void pm_install(page_manager_t *pm, page_t *page);

/**
 * @brief  Push a page onto the stack and make it active
 * @param  pm:    Pointer to the page manager
 * @param  name:  Name of the page to push (must be installed)
 * @param  stash: Pointer to stash descriptor (NULL = no stash)
 * @retval true on success, false if busy, not found, or duplicate
 * @note   Rejects pages already on the stack.
 *         The previous top gets sent through disappear sequence.
 *         Pass stash=NULL if no parameters need to be passed.
 *         No-op if a switch is already in progress.
 *         Usage:
 * @code
 *   int type = SPORT_RUNNING;
 *   page_stash_t stash = page_stash_make(type);
 *   pm_push(&g_pm, "Run", &stash);   // with stash
 *   pm_push(&g_pm, "Home", NULL);    // without stash
 * @endcode
 */
bool pm_push(page_manager_t *pm, const char *name,
             const page_stash_t *stash);

/**
 * @brief  Pop the top page and restore the previous one
 * @param  pm: Pointer to the page manager
 * @retval true on success
 * @note   Refuses to pop the last remaining page.
 *         The popped page is cached (root kept) by default.
 *         No-op if a switch is already in progress.
 */
bool pm_pop(page_manager_t *pm);

/**
 * @brief  Replace the top page with a new one (no return path)
 * @param  pm:    Pointer to the page manager
 * @param  name:  Name of the replacement page (must be installed)
 * @param  stash: Pointer to stash descriptor (NULL = no stash)
 * @retval true on success
 * @note   The old page is force-unloaded (no cache). The new page
 *         occupies the same stack slot. Pass NULL for stash if no
 *         parameters need to be passed.
 *         No-op if a switch is already in progress.
 */
bool pm_replace(page_manager_t *pm, const char *name,
                const page_stash_t *stash);

/**
 * @brief  Pop all pages back to the root (bottom of stack)
 * @param  pm: Pointer to the page manager
 * @retval true on success
 * @note   The root page stays on the stack; all pages above it
 *         are removed and force-unloaded.
 *         No-op if a switch is already in progress.
 */
bool pm_back_home(page_manager_t *pm);

/**
 * @brief  Get the current top page
 * @param  pm: Pointer to the page manager
 * @return Pointer to the top page, or NULL if empty
 */
page_t *pm_top(page_manager_t *pm);

/**
 * @brief  Get stack depth
 * @param  pm: Pointer to the page manager
 * @return Number of pages on stack (0 if empty)
 */
int pm_depth(page_manager_t *pm);

/**
 * @brief  Set global default animation
 * @param  pm:    Pointer to the page manager
 * @param  anim:  Animation type (use 0 for LOAD_ANIM_OVER_LEFT default)
 * @param  time:  Duration in ms (use 0 for 500ms default)
 * @param  path:  Animation curve (use NULL for ease-out default)
 * @retval None
 * @note   Values >= LOAD_ANIM_LAST are clamped to LOAD_ANIM_NONE.
 */
void pm_set_global_anim(page_manager_t *pm, load_anim_t anim, uint16_t time,
                        lv_anim_path_cb_t path);

/**
 * @brief  Set a default style applied to every page root
 * @param  pm:    Pointer to the page manager
 * @param  style: Pointer to a static/global lv_style_t (NULL to clear)
 * @retval None
 * @note   The style must outlive the PageManager (static or global).
 *         Applied in exec_load after root creation.
 *         Use this to set width/height/bg_color on all pages.
 */
void pm_set_root_default_style(page_manager_t *pm, lv_style_t *style);

/**
 * @brief  Enable gesture-to-pop (drag to go back) on a page
 * @param  pm:   Pointer to the page manager
 * @param  page: Pointer to the page to enable drag on
 * @retval None
 * @note   The drag handler calls pm_pop() directly; no LV_EVENT_LEAVE
 *         handler is needed.  Drag direction is determined by the page's animation type
 *         (horizontal for LEFT/RIGHT, vertical for TOP/BOTTOM).
 */
void pm_root_enable_drag(page_manager_t *pm, page_t *page);

#ifdef __cplusplus
}
#endif

#endif /* PAGE_MANAGER_H */

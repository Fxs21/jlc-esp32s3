/**
 * @file page.c
 * @brief Page base implementation
 */
#include <inttypes.h>
#include "page.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define TAG "page"

void page_init(page_t *page, const char *name, page_vtable_t vtable)
{
    if (!page)
        return;

    memset(page, 0, sizeof(*page));
    page->name           = name;
    page->vtable         = vtable;
    page->priv.state     = PAGE_STATE_IDLE;
    page->priv.is_cached = false; /* set to true after first load */
}

void page_set_custom_anim(page_t *page, load_anim_t type, uint16_t time,
                          lv_anim_path_cb_t path)
{
    if (!page)
        return;

    page->priv.anim.override.active    = true;
    page->priv.anim.override.anim_type = type;
    page->priv.anim.override.anim_time = time;
    page->priv.anim.override.anim_path = path;
}

void page_clear_custom_anim(page_t *page)
{
    if (!page)
        return;

    page->priv.anim.override.active = false;
}

void page_set_force_cache(page_t *page, bool enable)
{
    if (!page)
        return;

    page->priv.force_cache = enable;
}

void page_set_disable_auto_cache(page_t *page, bool disable)
{
    if (!page)
        return;

    page->priv.disable_auto_cache = disable;
}

bool page_stash_pop(page_t *page, void *ptr, uint32_t size)
{
    if (!page || !ptr)
        return false;

    if (!page->priv.stash.ptr || page->priv.stash.size == 0) {
        LOG_W(TAG, "Page(%s) stash pop: no stash", page->name);
        return false;
    }

    if (page->priv.stash.size != size) {
        LOG_W(TAG, "Page(%s) stash pop: size mismatch (%" PRIu32 " != %" PRIu32 ")", page->name,
              page->priv.stash.size, size);
        return false;
    }

    memcpy(ptr, page->priv.stash.ptr, page->priv.stash.size);
    free(page->priv.stash.ptr);
    page->priv.stash.ptr  = NULL;
    page->priv.stash.size = 0;
    return true;
}

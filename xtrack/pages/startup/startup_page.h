/**
 * @file  startup_page.h
 * @brief Startup splash page — Presenter (wires View + Model)
 *
 * Shows an animated splash, then replaces itself with Dialplate.
 * No cache, no switch animation.
 *
 * Architecture (X-Track pattern):
 *   page_startup_t (Presenter)
 *     +-- startup_view_t  (pure UI)
 *     +-- startup_model_t (Account placeholder)
 */
#ifndef STARTUP_PAGE_H
#define STARTUP_PAGE_H

#include "page_manager/page.h"
#include "startup_view.h"
#include "startup_model.h"

typedef struct page_startup_t {
    page_t base;

    startup_view_t view;   /**< Splash UI widgets */
    startup_model_t model; /**< Data layer placeholder */
} page_startup_t;

/** Initialise the startup page (Presenter) */
void page_startup_init(page_startup_t *p, data_center_t *dc);

#endif /* STARTUP_PAGE_H */

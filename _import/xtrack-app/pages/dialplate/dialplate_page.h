/**
 * @file  dialplate_page.h
 * @brief Dialplate page — Presenter (wires View + Model)
 *
 * The root/home page of the application. Shows current time
 * from the "Clock" DataProc node.
 *
 * Architecture (X-Track pattern):
 *   page_dialplate_t (Presenter)
 *     +-- dialplate_view_t  (pure UI)
 *     +-- dialplate_model_t (Account + data)
 */
#ifndef DIALPLATE_PAGE_H
#define DIALPLATE_PAGE_H

#include "page_manager/page.h"
#include "dialplate_view.h"
#include "dialplate_model.h"

typedef struct page_dialplate_t {
    page_t base;

    dialplate_view_t view;   /**< UI widgets */
    dialplate_model_t model; /**< Data subscription */
} page_dialplate_t;

/** Initialise the dialplate page (Presenter) */
void page_dialplate_init(page_dialplate_t *p);

#endif /* DIALPLATE_PAGE_H */

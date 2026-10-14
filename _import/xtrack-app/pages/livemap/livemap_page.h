/**
 * @file  livemap_page.h
 * @brief LiveMap skeleton page
 */
#ifndef LIVEMAP_PAGE_H
#define LIVEMAP_PAGE_H

#include "page_manager/page.h"
#include "livemap_view.h"
#include "livemap_model.h"

typedef struct page_livemap_t {
    page_t base;
    livemap_view_t view;
    livemap_model_t model;

    /** Track state (first fix reference + conversion) */
    struct {
        double ref_lat;
        double ref_lon;
        bool has_ref;
        lv_coord_t origin_x;  /**< Screen center X */
        lv_coord_t origin_y;  /**< Screen center Y */
    } track;
} page_livemap_t;

void page_livemap_init(page_livemap_t *p, data_center_t *dc);

#endif /* LIVEMAP_PAGE_H */

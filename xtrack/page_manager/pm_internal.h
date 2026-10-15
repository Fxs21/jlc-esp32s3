#ifndef PM_INTERNAL_H
#define PM_INTERNAL_H

#include "page_manager.h"

void pm_state_update(page_manager_t *pm, page_t *page);

void pm_anim_start(page_t *page);

load_anim_t page_anim_type(page_t *page);
uint16_t page_anim_time(page_t *page);
lv_anim_path_cb_t page_anim_path(page_t *page);

#endif /* PM_INTERNAL_H */

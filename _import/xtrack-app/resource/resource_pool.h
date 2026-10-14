#ifndef RESOURCE_POOL_H
#define RESOURCE_POOL_H

#include <lvgl.h>
#include <stddef.h>

void resource_pool_init(void);

const lv_img_dsc_t *resource_pool_get_image(const char *name);

#endif /* RESOURCE_POOL_H */

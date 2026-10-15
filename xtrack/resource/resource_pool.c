#include "resource_pool.h"
#include "images.h"
#include <string.h>

typedef struct {
    const char *name;
    const void *ptr;
} resource_entry_t;

#define IMG_ENTRY(name) {#name, &name}

static const resource_entry_t s_images[] = {
    IMG_ENTRY(alarm),
    IMG_ENTRY(battery),
    IMG_ENTRY(battery_info),
    IMG_ENTRY(bicycle),
    IMG_ENTRY(compass),
    IMG_ENTRY(gps_arrow_dark),
    IMG_ENTRY(gps_arrow_default),
    IMG_ENTRY(gps_arrow_light),
    IMG_ENTRY(gyroscope),
    IMG_ENTRY(info),
    IMG_ENTRY(locate),
    IMG_ENTRY(map_location),
    IMG_ENTRY(menu),
    IMG_ENTRY(origin_point),
    IMG_ENTRY(pause),
    IMG_ENTRY(satellite),
    IMG_ENTRY(sd_card),
    IMG_ENTRY(start),
    IMG_ENTRY(stop),
    IMG_ENTRY(storage),
    IMG_ENTRY(system_info),
    IMG_ENTRY(time_info),
    IMG_ENTRY(trip),
    {NULL, NULL},
};

static const resource_entry_t *find_entry(const resource_entry_t *table,
                                          const char *name)
{
    for (int i = 0; table[i].name != NULL; i++)
    {
        if (strcmp(name, table[i].name) == 0)
            return &table[i];
    }
    return NULL;
}

void resource_pool_init(void)
{
    /* Nothing needed at init — table is compiled-in */
}

const lv_img_dsc_t *resource_pool_get_image(const char *name)
{
    const resource_entry_t *e = find_entry(s_images, name);
    return e ? (const lv_img_dsc_t *)e->ptr : NULL;
}

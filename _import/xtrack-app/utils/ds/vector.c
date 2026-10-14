/**
 * @file  utils/vector.c
 * @brief Simple dynamic pointer array — implementation
 */
#include "vector.h"
#include <stdlib.h>
#include <string.h>

void vector_init(vector_t *v)
{
    v->items    = NULL;
    v->count    = 0;
    v->capacity = 0;
}

int vector_push(vector_t *v, void *item)
{
    if (v->count >= v->capacity) {
        int new_cap = v->capacity ? v->capacity * 2 : 4;
        void **new_items =
            (void **)realloc(v->items, (size_t)new_cap * sizeof(void *));
        if (!new_items)
            return -1;
        v->items    = new_items;
        v->capacity = new_cap;
    }

    v->items[v->count++] = item;
    return 0;
}

void vector_remove(vector_t *v, void *item)
{
    for (int i = 0; i < v->count; i++) {
        if (v->items[i] != item)
            continue;

        int tail = v->count - i - 1;
        if (tail > 0)
            memmove(&v->items[i], &v->items[i + 1],
                    (size_t)tail * sizeof(void *));
        v->count--;
        return;
    }
}

bool vector_contains(vector_t *v, void *item)
{
    for (int i = 0; i < v->count; i++)
        if (v->items[i] == item)
            return true;
    return false;
}

void vector_free(vector_t *v)
{
    free(v->items);
    v->items    = NULL;
    v->count    = 0;
    v->capacity = 0;
}

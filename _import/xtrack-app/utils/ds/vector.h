/**
 * @file  utils/vector.h
 * @brief Simple dynamic pointer array (C vector)
 *
 * Stores void* items with automatic reallocation.
 * No duplication checks — the caller is responsible for
 * checking vector_contains() before vector_push() if needed.
 */
#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void **items;
    int count;
    int capacity;
} vector_t;

/**
 * @brief  Initialize an empty vector
 * @param  v: Pointer to a vector_t (must not be NULL)
 * @note   items = NULL, count = 0, capacity = 0.
 *         Calling vector_free() or vector_push() on an
 *         uninitialized vector is undefined — always call
 *         vector_init() first.
 */
void vector_init(vector_t *v);

/**
 * @brief  Append an item to the end
 * @param  v:    Pointer to an initialized vector_t
 * @param  item: Pointer to append
 * @retval 0 on success, -1 on allocation failure
 * @note   Capacity doubles automatically when full
 *         (starting from 4 on first push).
 */
int vector_push(vector_t *v, void *item);

/**
 * @brief  Remove an item by pointer value, preserving order
 * @param  v:    Pointer to an initialized vector_t
 * @param  item: Pointer to remove (compared by address)
 * @note   Linear scan, O(n). Subsequent elements are shifted
 *         left via memmove to preserve insertion order.
 *         If item is not found, nothing happens.
 */
void vector_remove(vector_t *v, void *item);

/**
 * @brief  Check if an item exists in the vector
 * @param  v:    Pointer to an initialized vector_t
 * @param  item: Pointer to find (compared by address)
 * @retval true if found, false otherwise
 */
bool vector_contains(vector_t *v, void *item);

/**
 * @brief  Free internal storage and reset to empty state
 * @param  v: Pointer to an initialized vector_t
 * @note   After vector_free(), the vector is reusable without
 *         calling vector_init() again (items = NULL, count = 0).
 */
void vector_free(vector_t *v);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_H */

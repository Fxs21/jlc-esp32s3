/**
 * @file data_center.c
 * @brief DataCenter - named registry of Account endpoints
 *
 * Manages the account pool and provides lookup/registration.
 * Account API (subscribe, commit, publish, etc.) lives in account.c.
 *
 * Accounts are heap-allocated (no fixed pool).
 * The main account is embedded in the data_center_t struct (not in pool).
 */
#include "data_center.h"
#include "utils/ds/vector.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "data_center"

data_center_t *data_center_create(void)
{
    data_center_t *data_center =
        (data_center_t *)calloc(1, sizeof(data_center_t));
    if (!data_center) {
        LOG_E(TAG, "data_center_create: calloc failed");
        return NULL;
    }

    /* Init embedded main account */
    data_center->main_account.id          = "main";
    data_center->main_account.data_center = data_center;

    return data_center;
}

void data_center_destroy(data_center_t *data_center)
{
    if (!data_center)
        return;

    /* Destroy all pool accounts — account_destroy removes each from pool */
    while (data_center->pool.count > 0) {
        account_t *account = (account_t *)data_center->pool.items[0];
        account_destroy(account);
    }
    vector_free(&data_center->pool);

    /* Embedded main account: pool accounts already disconnected the
     * bidirectional links via account_destroy. Only need to free the
     * vector items arrays (publishers was never used, but safe to free) */
    vector_free(&data_center->main_account.publishers);
    vector_free(&data_center->main_account.subscribers);

    free(data_center);
}

account_t *data_center_find_account(data_center_t *data_center, const char *id)
{
    if (!data_center || !id)
        return NULL;

    /* Check main account first (embedded, never NULL) */
    if (strcmp(data_center->main_account.id, id) == 0)
        return &data_center->main_account;

    /* Search pool */
    for (int i = 0; i < data_center->pool.count; i++) {
        account_t *account = (account_t *)data_center->pool.items[i];
        if (strcmp(account->id, id) == 0)
            return account;
    }
    return NULL;
}

account_err_t data_center_add_account(data_center_t *data_center,
                                      account_t *account)
{
    if (!data_center || !account)
        return ACCOUNT_ERR_PARAM;
    if (!account->id)
        return ACCOUNT_ERR_PARAM;

    if (strcmp(account->id, data_center->main_account.id) == 0) {
        LOG_E(TAG, "cannot add reserved id: %s", account->id);
        return ACCOUNT_ERR_NOT_FOUND;
    }

    if (data_center_find_account(data_center, account->id)) {
        LOG_E(TAG, "duplicate id: %s", account->id);
        return ACCOUNT_ERR_NOT_FOUND;
    }

    if (vector_push(&data_center->pool, account) != 0)
        return ACCOUNT_FAIL;

    /* Embedded main account auto-subscribes to every new account */
    vector_push(&account->subscribers, &data_center->main_account);
    vector_push(&data_center->main_account.publishers, account);

    return ACCOUNT_OK;
}

void data_center_remove_account(data_center_t *data_center, account_t *account)
{
    if (!data_center || !account)
        return;

    /* Simply remove from pool — main_account is embedded, not in pool,
     * so passing &data_center->main_account here is harmless */
    vector_remove(&data_center->pool, account);
}

/**
 * @file  data_center.h
 * @brief DataCenter - named registry of Account endpoints
 *
 * Manages the account pool and provides lookup/registration.
 * Account API (subscribe, commit, publish, etc.) lives in account.h.
 *
 * The main account (id "main") is embedded in the data_center_t struct
 * at data_center_create() time and is NOT in the pool vector.
 * main_account auto-subscribes to every newly registered account.
 *
 * Fields are fully public (same design as X-Track).
 */
#ifndef DATA_CENTER_H
#define DATA_CENTER_H

#include "account.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct data_center_t {
    vector_t pool;          /**< Registered accounts (excluding main) */
    account_t main_account; /**< Embedded main account (auto-subscribed to all) */
} data_center_t;

/**
 * @brief  Create a DataCenter and its embedded main account
 * @retval Pointer to the new DataCenter, or NULL on allocation failure
 * @note   Creates an embedded main account with id "main" (no separate
 *         heap allocation — it lives inside the data_center_t struct).
 *         The main account is NOT in pool and auto-subscribes to every
 *         account registered via data_center_add_account().
 */
data_center_t *data_center_create(void);

/**
 * @brief  Destroy a DataCenter and all its accounts
 * @param  data_center: Pointer to the DataCenter to destroy
 * @note   Destroys all pool accounts first, then cleans up the embedded
 *         main account (no free — it is part of the struct),
 *         then frees the DataCenter itself. Accounts are removed from
 *         their subscribers' publisher lists during cleanup.
 *         After this call the pointer is invalid.
 */
void data_center_destroy(data_center_t *data_center);

/**
 * @brief  Find an account by its id string
 * @param  data_center: Pointer to the DataCenter
 * @param  id:          Account id string to search for
 * @retval Pointer to the account, or NULL if not found or params are NULL
 * @note   Searches the main_account first, then the pool vector.
 */
account_t *data_center_find_account(data_center_t *data_center, const char *id);

/**
 * @brief  Register an account in the DataCenter
 * @param  data_center: Pointer to the DataCenter
 * @param  account:     Pointer to the account to register
 * @retval ACCOUNT_OK on success
 * @retval ACCOUNT_ERR_PARAM if data_center or account is NULL, or account
 *         has no id
 * @retval ACCOUNT_ERR_NOT_FOUND if account id is reserved ("main")
 *         or a duplicate
 * @retval ACCOUNT_FAIL if pool vector push fails
 * @note   On success, the main_account is auto-subscribed to the new
 *         account (added to each other's subscriber/publisher lists).
 */
account_err_t data_center_add_account(data_center_t *data_center,
                                      account_t *account);

/**
 * @brief  Unregister an account from the DataCenter
 * @param  data_center: Pointer to the DataCenter
 * @param  account:     Pointer to the account to remove
 * @note   Removes the account from the pool vector. The embedded
 *         main_account is never in the pool, so passing its address
 *         is harmless (vector_remove does nothing if not found).
 *         The subscriber/publisher lists are NOT modified here -- call
 *         account_destroy() for proper cleanup.
 */
void data_center_remove_account(data_center_t *data_center, account_t *account);

#ifdef __cplusplus
}
#endif

#endif /* DATA_CENTER_H */

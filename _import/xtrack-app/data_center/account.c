/**
 * @file account.c
 * @brief Account API: lifecycle (create/destroy), subscribe/unsubscribe,
 *          commit/publish (broadcast), pull (lazy read), notify (one-to-one),
 *          callback and timer control
 */
#include <inttypes.h>
#include "data_center.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "account"

static void timer_cb(lv_timer_t *timer)
{
    account_t *account = (account_t *)lv_timer_get_user_data(timer);
    if (!account)
        return;

    if (!account->priv.callback)
        return;

    account_event_param_t param;
    param.event = ACCOUNT_EVENT_TIMER;
    param.tran  = account;
    param.recv  = account;
    param.data  = NULL;
    param.size  = 0;
    account->priv.callback(account, &param);
}

account_t *account_create(data_center_t *data_center, const char *id,
                          uint32_t buf_size, void *user_data)
{
    if (!data_center || !id)
        return NULL;

    LOG_I(TAG, "Account[%s] creating...", id);

    account_t *account = (account_t *)calloc(1, sizeof(account_t));
    if (!account) {
        LOG_E(TAG, "%s: calloc failed", id);
        return NULL;
    }

    account->id          = id;
    account->data_center = data_center;
    account->user_data   = user_data;

    if (buf_size > 0) {
        uint8_t *buf = (uint8_t *)malloc(buf_size);
        if (!buf) {
            LOG_E(TAG, "%s: malloc(%" PRIu32 ") failed", id, buf_size);
            account_destroy(account);
            return NULL;
        }
        memset(buf, 0, buf_size);
        account->priv.value_buf  = buf;
        account->priv.value_size = buf_size;
    }

    if (data_center_add_account(data_center, account) != ACCOUNT_OK) {
        account_destroy(account);
        return NULL;
    }

    LOG_I(TAG, "Account[%s] created", account->id);
    return account;
}

void account_destroy(account_t *account)
{
    if (!account)
        return;

    LOG_I(TAG, "Account[%s] deleting...", account->id);

    /* Free the latest-value buffer */
    if (account->priv.value_buf)
        free(account->priv.value_buf);

    /* Delete timer */
    if (account->priv.timer) {
        lv_timer_del(account->priv.timer);
    }

    /* Disconnect subscribers: tell each subscriber to unsubscribe from us */
    while (account->subscribers.count > 0) {
        account_t *subscriber = (account_t *)account->subscribers.items[0];
        account_unsubscribe(subscriber, account->id);
    }

    /* Disconnect publishers: remove ourselves from each publisher's subscribers
     */
    while (account->publishers.count > 0) {
        account_t *publisher = (account_t *)account->publishers.items[0];
        vector_remove(&publisher->subscribers, account);
        vector_remove(&account->publishers, publisher);
    }

    /* Remove from DataCenter pool */
    if (account->data_center)
        data_center_remove_account(account->data_center, account);

    /* Free subscriber/publisher vectors */
    vector_free(&account->subscribers);
    vector_free(&account->publishers);

    LOG_I(TAG, "Account[%s] deleted", account->id);
    free(account);
}

account_t *account_subscribe(account_t *self, const char *pub_id)
{
    if (!self || !pub_id)
        return NULL;
    if (!self->data_center)
        return NULL;

    if (strcmp(pub_id, self->id) == 0) {
        LOG_E(TAG, "%s tried to subscribe to itself", self->id);
        return NULL;
    }

    account_t *publisher = data_center_find_account(self->data_center, pub_id);
    if (!publisher)
        return NULL;

    if (vector_contains(&self->publishers, publisher)) {
        LOG_E(TAG, "subscriber[%s] multi subscribe publisher[%s]", self->id,
              pub_id);
        return NULL;
    }

    if (vector_push(&publisher->subscribers, self) != 0)
        return NULL;

    if (vector_push(&self->publishers, publisher) != 0) {
        vector_remove(&publisher->subscribers, self);
        return NULL;
    }

    LOG_I(TAG, "subscriber[%s] subscribed publisher[%s]", self->id, pub_id);
    return publisher;
}

account_err_t account_unsubscribe(account_t *self, const char *pub_id)
{
    if (!self || !pub_id)
        return ACCOUNT_ERR_PARAM;
    if (!self->data_center)
        return ACCOUNT_FAIL;

    account_t *publisher = data_center_find_account(self->data_center, pub_id);
    if (!publisher)
        return ACCOUNT_ERR_NOT_FOUND;

    if (!vector_contains(&self->publishers, publisher)) {
        LOG_W(TAG, "subscriber[%s] was not subscribed publisher[%s]", self->id,
              pub_id);
        return ACCOUNT_ERR_NOT_FOUND;
    }

    vector_remove(&self->publishers, publisher);
    vector_remove(&publisher->subscribers, self);
    return ACCOUNT_OK;
}

account_err_t account_commit(account_t *self, const void *data, uint32_t size)
{
    if (!self || !data)
        return ACCOUNT_ERR_PARAM;
    if (self->priv.value_size == 0)
        return ACCOUNT_ERR_NO_CACHE;
    if (size != self->priv.value_size)
        return ACCOUNT_ERR_SIZE;

    memcpy(self->priv.value_buf, data, size);
    self->priv.has_value = true;
    return ACCOUNT_OK;
}

account_err_t account_publish(account_t *self)
{
    if (!self)
        return ACCOUNT_ERR_PARAM;
    if (self->priv.value_size == 0)
        return ACCOUNT_ERR_NO_CACHE;
    if (!self->priv.has_value)
        return ACCOUNT_ERR_NO_DATA;

    account_err_t retval = ACCOUNT_FAIL;

    for (int i = 0; i < self->subscribers.count; i++) {
        account_t *subscriber = (account_t *)self->subscribers.items[i];
        if (!subscriber || !subscriber->priv.callback) {
            continue;
        }

        account_event_param_t param;
        param.event = ACCOUNT_EVENT_PUB_PUBLISH;
        param.tran  = self;
        param.recv  = subscriber;
        param.data  = self->priv.value_buf;
        param.size  = self->priv.value_size;

        retval = subscriber->priv.callback(subscriber, &param);
    }

    return retval;
}

account_err_t account_pull(account_t *self, const char *pub_id, void *data,
                           uint32_t size)
{
    if (!self || !pub_id || !data)
        return ACCOUNT_ERR_PARAM;
    if (!self->data_center)
        return ACCOUNT_FAIL;

    account_t *publisher = data_center_find_account(self->data_center, pub_id);
    if (!publisher)
        return ACCOUNT_ERR_NOT_FOUND;

    if (!vector_contains(&self->publishers, publisher))
        return ACCOUNT_ERR_NOT_FOUND;

    /* Try publisher's callback first */
    if (publisher->priv.callback) {
        account_event_param_t param;
        param.event = ACCOUNT_EVENT_SUB_PULL;
        param.tran  = self;
        param.recv  = publisher;
        param.data  = data;
        param.size  = size;

        account_err_t ret = publisher->priv.callback(publisher, &param);
        return ret;
    }

    /* Fallback: read the latest committed value directly */
    if (publisher->priv.value_size != 0) {
        if (publisher->priv.value_size != size) {
            LOG_E(TAG, "data size publisher[%s]:%" PRIu32 " != subscriber[%s]:%" PRIu32 ",",
                  publisher->id, publisher->priv.value_size, self->id, size);
            return ACCOUNT_ERR_SIZE;
        }

        if (publisher->priv.has_value) {
            memcpy(data, publisher->priv.value_buf, size);
            return ACCOUNT_OK;
        }
        LOG_W(TAG, "publisher[%s] data was not committed", publisher->id);
        return ACCOUNT_ERR_NO_DATA;
    }

    return ACCOUNT_ERR_NO_CALLBACK;
}

account_err_t account_notify(account_t *self, const char *target_id,
                             const void *data, uint32_t size)
{
    if (!self || !target_id)
        return ACCOUNT_ERR_PARAM;
    if (!self->data_center)
        return ACCOUNT_FAIL;

    account_t *target = data_center_find_account(self->data_center, target_id);
    if (!target)
        return ACCOUNT_ERR_NOT_FOUND;

    if (!vector_contains(&self->publishers, target)) {
        LOG_W(TAG, "subscriber[%s] was not subscribed publisher[%s]", self->id,
              target_id);
        return ACCOUNT_ERR_NOT_FOUND;
    }

    if (!target->priv.callback) {
        LOG_W(TAG, "publisher[%s] not register callback", target_id);
        return ACCOUNT_ERR_NO_CALLBACK;
    }

    account_event_param_t param;
    param.event = ACCOUNT_EVENT_NOTIFY;
    param.tran  = self;
    param.recv  = target;
    param.data  = (void *)data;
    param.size  = size;

    account_err_t ret = target->priv.callback(target, &param);
    if (ret != ACCOUNT_OK)
        LOG_E(TAG, "send failed: %d", ret);
    return ret;
}

void account_set_timer_period(account_t *account, uint32_t period_ms)
{
    if (!account)
        return;

    if (account->priv.timer) {
        lv_timer_set_period(account->priv.timer, period_ms);
        return;
    }

    if (period_ms == 0)
        return;

    account->priv.timer = lv_timer_create(timer_cb, period_ms, account);
    lv_timer_pause(account->priv.timer);
}

void account_set_timer_enable(account_t *account, int enable)
{
    if (!account || !account->priv.timer)
        return;
    if (enable)
        lv_timer_resume(account->priv.timer);
    else
        lv_timer_pause(account->priv.timer);
}

void account_set_callback(account_t *account, account_cb_t cb)
{
    if (!account)
        return;
    account->priv.callback = cb;
}

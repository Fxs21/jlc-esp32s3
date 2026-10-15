/**
 * @file dialplate_model.c
 * @brief Dialplate Model — Account + Clock + GPS state
 */
#include "dialplate_model.h"
#include "log.h"
#include <string.h>

#define TAG "dialplate_model"

static int on_model_event(account_t *account, account_event_param_t *param)
{
    dialplate_model_t *m = (dialplate_model_t *)account->user_data;

    if (param->event != ACCOUNT_EVENT_PUB_PUBLISH)
        return ACCOUNT_ERR_UNSUPPORTED;

    /* Route by publisher ID */
    if (strcmp(param->tran->id, "Clock") == 0) {
        if (param->size != sizeof(hal_clock_info_t))
            return ACCOUNT_ERR_SIZE;
        memcpy(&m->clock_info, param->data, sizeof(hal_clock_info_t));
    } else if (strcmp(param->tran->id, "GPS") == 0) {
        if (param->size != sizeof(hal_gps_info_t))
            return ACCOUNT_ERR_SIZE;
        memcpy(&m->gps_info, param->data, sizeof(hal_gps_info_t));
    } else {
        return ACCOUNT_ERR_UNSUPPORTED;
    }

    if (m->event_cb)
        m->event_cb(m, param);

    return ACCOUNT_OK;
}

void dialplate_model_init(dialplate_model_t *m, data_center_t *dc)
{
    memset(m, 0, sizeof(*m));

    m->account = account_create(dc, "Dialplate", 0, m);
    if (!m->account) {
        LOG_E(TAG, "cannot create Dialplate account");
        return;
    }

    if (!account_subscribe(m->account, "Clock"))
        LOG_W(TAG, "cannot subscribe to Clock");
    if (!account_subscribe(m->account, "GPS"))
        LOG_W(TAG, "cannot subscribe to GPS");

    account_set_callback(m->account, on_model_event);
}

void dialplate_model_deinit(dialplate_model_t *m)
{
    if (m->account) {
        account_destroy(m->account);
        m->account = NULL;
    }
}

int dialplate_model_pull_clock(dialplate_model_t *m)
{
    if (!m->account)
        return ACCOUNT_ERR_PARAM;
    return account_pull(m->account, "Clock", &m->clock_info,
                        sizeof(m->clock_info));
}

int dialplate_model_pull_gps(dialplate_model_t *m)
{
    if (!m->account)
        return ACCOUNT_ERR_PARAM;
    return account_pull(m->account, "GPS", &m->gps_info,
                        sizeof(m->gps_info));
}

/**
 * @file livemap_model.c
 * @brief LiveMap Model — subscribe to GPS
 */
#include "livemap_model.h"
#include "log.h"
#include <string.h>

#define TAG "livemap_model"

static int on_model_event(account_t *account, account_event_param_t *param)
{
    livemap_model_t *m = (livemap_model_t *)account->user_data;

    if (param->event != ACCOUNT_EVENT_PUB_PUBLISH)
        return ACCOUNT_ERR_UNSUPPORTED;

    if (strcmp(param->tran->id, "GPS") != 0)
        return ACCOUNT_ERR_UNSUPPORTED;

    if (param->size != sizeof(hal_gps_info_t))
        return ACCOUNT_ERR_SIZE;

    memcpy(&m->gps_info, param->data, sizeof(hal_gps_info_t));
    return ACCOUNT_OK;
}

void livemap_model_init(livemap_model_t *m, data_center_t *dc)
{
    memset(m, 0, sizeof(*m));

    m->account = account_create(dc, "LiveMap", 0, m);
    if (!m->account) {
        LOG_E(TAG, "cannot create LiveMap account");
        return;
    }

    if (!account_subscribe(m->account, "GPS"))
        LOG_W(TAG, "cannot subscribe to GPS");

    account_set_callback(m->account, on_model_event);
}

void livemap_model_deinit(livemap_model_t *m)
{
    if (m->account) {
        account_destroy(m->account);
        m->account = NULL;
    }
}

int livemap_model_pull_gps(livemap_model_t *m)
{
    if (!m->account)
        return ACCOUNT_ERR_PARAM;
    return account_pull(m->account, "GPS", &m->gps_info,
                        sizeof(m->gps_info));
}

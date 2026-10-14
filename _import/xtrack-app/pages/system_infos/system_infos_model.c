/**
 * @file system_infos_model.c
 * @brief SystemInfos Model — Account + Clock + GPS + Power data
 */
#include "system_infos_model.h"
#include "log.h"
#include <string.h>

#define TAG "system_infos_model"

static int on_model_event(account_t *account, account_event_param_t *param)
{
    system_infos_model_t *m = (system_infos_model_t *)account->user_data;

    if (param->event != ACCOUNT_EVENT_PUB_PUBLISH)
        return ACCOUNT_ERR_UNSUPPORTED;

    if (strcmp(param->tran->id, "Clock") == 0) {
        if (param->size != sizeof(hal_clock_info_t))
            return ACCOUNT_ERR_SIZE;
        memcpy(&m->clock_info, param->data, sizeof(hal_clock_info_t));
    } else if (strcmp(param->tran->id, "GPS") == 0) {
        if (param->size != sizeof(hal_gps_info_t))
            return ACCOUNT_ERR_SIZE;
        memcpy(&m->gps_info, param->data, sizeof(hal_gps_info_t));
    } else if (strcmp(param->tran->id, "Power") == 0) {
        if (param->size != sizeof(hal_power_info_t))
            return ACCOUNT_ERR_SIZE;
        memcpy(&m->power_info, param->data, sizeof(hal_power_info_t));
    } else {
        return ACCOUNT_ERR_UNSUPPORTED;
    }

    if (m->event_cb)
        m->event_cb(m, param);

    return ACCOUNT_OK;
}

void system_infos_model_init(system_infos_model_t *m, data_center_t *dc)
{
    memset(m, 0, sizeof(*m));

    m->account = account_create(dc, "SystemInfos", 0, m);
    if (!m->account) {
        LOG_E(TAG, "cannot create SystemInfos account");
        return;
    }

    if (!account_subscribe(m->account, "Clock"))
        LOG_W(TAG, "cannot subscribe to Clock");
    if (!account_subscribe(m->account, "GPS"))
        LOG_W(TAG, "cannot subscribe to GPS");
    if (!account_subscribe(m->account, "Power"))
        LOG_W(TAG, "cannot subscribe to Power");

    account_set_callback(m->account, on_model_event);
}

void system_infos_model_deinit(system_infos_model_t *m)
{
    if (m->account) {
        account_destroy(m->account);
        m->account = NULL;
    }
}

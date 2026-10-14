/**
 * @file dp_power.c
 * @brief DataProc node — Power/Battery
 *
 * Periodically reads HAL simulated battery and publishes
 * the power status to all subscribers.
 */
#include "data_proc.h"
#include "hal/hal_power.h"
#include "log.h"
#include <string.h>

#define TAG "dp_power"

/* Publish every 3 seconds (battery changes slowly) */
#define POWER_PUBLISH_PERIOD_MS 3000

static int on_power_event(account_t *account, account_event_param_t *param)
{
    switch (param->event) {
    case ACCOUNT_EVENT_TIMER: {
        hal_power_info_t power;
        hal_power_get_info(&power);

        account_err_t ret = account_commit(account, &power, sizeof(power));
        if (ret != ACCOUNT_OK) {
            LOG_W(TAG, "commit failed: %d", ret);
            return ret;
        }

        ret = account_publish(account);
        if (ret != ACCOUNT_OK && ret != ACCOUNT_FAIL) {
            LOG_W(TAG, "publish failed: %d", ret);
        }
        return ACCOUNT_OK;
    }

    case ACCOUNT_EVENT_SUB_PULL: {
        if (param->size != sizeof(hal_power_info_t))
            return ACCOUNT_ERR_SIZE;
        hal_power_get_info((hal_power_info_t *)param->data);
        return ACCOUNT_OK;
    }

    default:
        return ACCOUNT_ERR_UNSUPPORTED;
    }
}

void dp_power_init(account_t *account)
{
    account_set_callback(account, on_power_event);
    account_set_timer_period(account, POWER_PUBLISH_PERIOD_MS);
    account_set_timer_enable(account, 1);

    LOG_I(TAG, "Power node initialised");
}

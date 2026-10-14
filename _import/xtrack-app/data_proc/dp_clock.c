/**
 * @file dp_clock.c
 * @brief DataProc node — Clock
 *
 * Periodically reads HAL system clock and publishes the time
 * to all subscribers via Commit + Publish.
 */
#include "data_proc.h"
#include "hal/hal_clock.h"
#include "log.h"
#include <string.h>

#define TAG "dp_clock"

/* Timer interval (ms) — publish once per second */
#define CLOCK_PUBLISH_PERIOD_MS 1000

static int on_clock_event(account_t *account, account_event_param_t *param)
{
    switch (param->event) {
    case ACCOUNT_EVENT_TIMER: {
        /* Read HAL time and publish */
        hal_clock_info_t clock;
        hal_clock_get_info(&clock);

        account_err_t ret = account_commit(account, &clock, sizeof(clock));
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
        if (param->size != sizeof(hal_clock_info_t))
            return ACCOUNT_ERR_SIZE;
        hal_clock_get_info((hal_clock_info_t *)param->data);
        return ACCOUNT_OK;
    }

    default:
        return ACCOUNT_ERR_UNSUPPORTED;
    }
}

void dp_clock_init(account_t *account)
{
    account_set_callback(account, on_clock_event);
    account_set_timer_period(account, CLOCK_PUBLISH_PERIOD_MS);
    account_set_timer_enable(account, 1);

    LOG_I(TAG, "Clock node initialised");
}

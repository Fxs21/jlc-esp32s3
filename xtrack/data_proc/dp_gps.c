/**
 * @file dp_gps.c
 * @brief DataProc node — GPS (simulated)
 *
 * Generates fake GPS data simulating a moving vehicle/person.
 * Position oscillates around Beijing (39.9N, 116.4E) with
 * varying speed, course, and altitude.
 */
#include "data_proc.h"
#include "hal/hal_gps.h"
#include "log.h"
#include <string.h>
#include <math.h>

#define TAG "dp_gps"

#define PUBLISH_PERIOD_MS 1000
#define PI                3.14159265f

static hal_gps_info_t s_last_gps; /* Cache last generated value for SUB_PULL */

static int on_gps_event(account_t *account, account_event_param_t *param)
{
    switch (param->event) {
    case ACCOUNT_EVENT_TIMER: {
        static int tick = 0;
        hal_gps_info_t gps;

        tick++;

        /* Oscillate lat/lng to simulate slow circular movement.
         * Period ~60 seconds, radius ~0.002 deg (~200m). */
        float angle = (float)tick * 2.0f * PI / 60.0f;
        gps.latitude  = 39.9 + 0.002 * sinf(angle);
        gps.longitude = 116.4 + 0.002 * cosf(angle);

        /* Speed varies with position: faster at the "straights" */
        gps.speed = 20.0f + 30.0f * (sinf(angle * 2.0f) * 0.5f + 0.5f);

        /* Course follows movement direction */
        gps.course = angle * 180.0f / PI;

        /* Altitude with gentle variation */
        gps.altitude = 50.0f + 5.0f * sinf(angle * 0.5f);

        /* Satellite count fluctuates */
        gps.satellites = 8 + (int)(sinf(angle * 3.0f) * 2.0f + 0.5f);

        gps.is_valid = 1;

        /* Cache for SUB_PULL */
        s_last_gps = gps;

        account_err_t ret = account_commit(account, &gps, sizeof(gps));
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
        if (param->size != sizeof(hal_gps_info_t))
            return ACCOUNT_ERR_SIZE;
        memcpy(param->data, &s_last_gps, sizeof(s_last_gps));
        return ACCOUNT_OK;
    }

    default:
        return ACCOUNT_ERR_UNSUPPORTED;
    }
}

void dp_gps_init(account_t *account)
{
    memset(&s_last_gps, 0, sizeof(s_last_gps));
    account_set_callback(account, on_gps_event);
    account_set_timer_period(account, PUBLISH_PERIOD_MS);
    account_set_timer_enable(account, 1);

    LOG_I(TAG, "GPS node initialised");
}

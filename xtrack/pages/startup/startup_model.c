/**
 * @file startup_model.c
 * @brief Startup Model — init/deinit + StatusBar control
 *
 * Creates an Account and subscribes to "StatusBar" to send
 * the APPEAR command when the startup page finishes.
 */
#include "startup_model.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

#define TAG "startup"

void startup_model_init(startup_model_t *m, data_center_t *dc)
{
    if (!m || !dc)
        return;

    m->account = account_create(dc, "StartupModel", 0, NULL);
    if (!m->account) {
        LOG_E(TAG, "StartupModel account_create failed");
        return;
    }

    /* Subscribe to StatusBar so we can send it Appear commands */
    if (!account_subscribe(m->account, "StatusBar")) {
        LOG_W(TAG, "cannot subscribe to StatusBar");
    }

    LOG_I(TAG, "StartupModel init ok");
}

void startup_model_deinit(startup_model_t *m)
{
    if (!m)
        return;

    if (m->account) {
        account_destroy(m->account);
        m->account = NULL;
    }
}

void startup_model_show_status_bar(startup_model_t *m)
{
    if (!m || !m->account)
        return;

    status_bar_info_t info;
    memset(&info, 0, sizeof(info));
    info.cmd          = STATUS_BAR_CMD_APPEAR;
    info.param.appear = true;

    account_notify(m->account, "StatusBar", &info, sizeof(info));
}

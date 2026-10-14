/**
 * @file  system_infos_model.h
 * @brief SystemInfos Model — data layer (Account + system state)
 */
#ifndef SYSTEM_INFOS_MODEL_H
#define SYSTEM_INFOS_MODEL_H

#include "data_center/data_center.h"
#include "hal/hal_clock.h"
#include "hal/hal_gps.h"
#include "hal/hal_power.h"

typedef struct system_infos_model_t system_infos_model_t;

/** Internal event callback type */
typedef int (*system_infos_model_event_cb_t)(system_infos_model_t *model,
                                              account_event_param_t *param);

struct system_infos_model_t {
    account_t *account;                    /**< Our subscription account */
    hal_clock_info_t clock_info;           /**< Latest clock data */
    hal_gps_info_t   gps_info;             /**< Latest GPS data */
    hal_power_info_t power_info;           /**< Latest power data */
    system_infos_model_event_cb_t event_cb;
};

/** Initialise Model: create Account, subscribe to Clock, GPS, Power */
void system_infos_model_init(system_infos_model_t *m, data_center_t *dc);

/** Deinitialise Model */
void system_infos_model_deinit(system_infos_model_t *m);

#endif /* SYSTEM_INFOS_MODEL_H */

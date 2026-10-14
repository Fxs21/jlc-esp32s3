/**
 * @file  dialplate_model.h
 * @brief Dialplate Model — data layer (Clock + GPS + sport state)
 */
#ifndef DIALPLATE_MODEL_H
#define DIALPLATE_MODEL_H

#include "data_center/data_center.h"
#include "hal/hal_clock.h"
#include "hal/hal_gps.h"

typedef struct dialplate_model_t dialplate_model_t;

/** Internal event callback type */
typedef int (*dialplate_model_event_cb_t)(dialplate_model_t *model,
                                          account_event_param_t *param);

struct dialplate_model_t {
    account_t *account;                     /**< Our subscription account */

    hal_clock_info_t clock_info;            /**< Latest clock data */
    hal_gps_info_t   gps_info;              /**< Latest GPS data */

    float avg_speed;                        /**< Computed average speed */
    float trip_distance;                    /**< Trip distance (km) placeholder */
    int   trip_time_sec;                    /**< Trip time (seconds) placeholder */
    int   calories;                         /**< Calories placeholder */

    dialplate_model_event_cb_t event_cb;    /**< External notification */
};

/** Initialise Model: create Account, subscribe to "Clock" + "GPS" */
void dialplate_model_init(dialplate_model_t *m, data_center_t *dc);

/** Deinitialise Model: destroy Account */
void dialplate_model_deinit(dialplate_model_t *m);

/** Pull current clock data */
int dialplate_model_pull_clock(dialplate_model_t *m);

/** Pull current GPS data */
int dialplate_model_pull_gps(dialplate_model_t *m);

#endif /* DIALPLATE_MODEL_H */

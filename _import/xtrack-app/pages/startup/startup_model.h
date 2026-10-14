/**
 * @file  startup_model.h
 * @brief Startup Model — data layer (Account + StatusBar control)
 */
#ifndef STARTUP_MODEL_H
#define STARTUP_MODEL_H

#include "data_center/data_center.h"
#include "pages/status_bar/status_bar.h"

typedef struct {
    account_t *account; /**< Our subscription account */
} startup_model_t;

/** Initialise Model: create Account on DataCenter */
void startup_model_init(startup_model_t *m, data_center_t *dc);

/** Deinitialise Model: destroy Account */
void startup_model_deinit(startup_model_t *m);

/** Show the StatusBar overlay via Account Notify */
void startup_model_show_status_bar(startup_model_t *m);

#endif /* STARTUP_MODEL_H */

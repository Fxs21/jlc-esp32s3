/**
 * @file  livemap_model.h
 * @brief LiveMap Model — GPS data
 */
#ifndef LIVEMAP_MODEL_H
#define LIVEMAP_MODEL_H

#include "data_center/data_center.h"
#include "hal/hal_gps.h"

typedef struct {
    account_t *account;
    hal_gps_info_t gps_info;
} livemap_model_t;

void livemap_model_init(livemap_model_t *m, data_center_t *dc);
void livemap_model_deinit(livemap_model_t *m);
int  livemap_model_pull_gps(livemap_model_t *m);

#endif /* LIVEMAP_MODEL_H */

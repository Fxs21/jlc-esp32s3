#include "bsp_pmu.h"

static const bsp_pmu_desc_t s_desc = {
    .present = false,
};

const bsp_pmu_desc_t *bsp_pmu_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_pmu_open(const bsp_pmu_config_t *config, bsp_pmu_handle_t *out_pmu)
{
    (void)config;
    if (out_pmu != NULL) {
        *out_pmu = NULL;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_pmu_close(bsp_pmu_handle_t pmu)
{
    (void)pmu;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_pmu_get_status(bsp_pmu_handle_t pmu, bsp_pmu_status_t *out_status)
{
    (void)pmu;
    (void)out_status;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_pmu_get_events(bsp_pmu_handle_t pmu, bsp_pmu_event_t *out_events, bool clear)
{
    (void)pmu;
    (void)out_events;
    (void)clear;
    return ESP_ERR_NOT_SUPPORTED;
}

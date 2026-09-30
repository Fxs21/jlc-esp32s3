#include "bsp_rtc.h"

static const bsp_rtc_desc_t s_desc = {
    .present = false,
};

const bsp_rtc_desc_t *bsp_rtc_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_rtc_open(bsp_rtc_handle_t *rtc_out)
{
    if (rtc_out != NULL) {
        *rtc_out = NULL;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_rtc_close(bsp_rtc_handle_t rtc)
{
    (void)rtc;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_rtc_get_time(bsp_rtc_handle_t rtc, bsp_rtc_time_t *time_out, bool *valid_out)
{
    (void)rtc;
    (void)time_out;
    (void)valid_out;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_rtc_set_time(bsp_rtc_handle_t rtc, const bsp_rtc_time_t *time)
{
    (void)rtc;
    (void)time;
    return ESP_ERR_NOT_SUPPORTED;
}

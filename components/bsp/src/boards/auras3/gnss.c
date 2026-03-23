#include "bsp_gnss.h"

#include "esp_check.h"

#define TAG "bsp_gnss"

static const bsp_gnss_desc_t s_desc = {
    .present = false,
};

const bsp_gnss_desc_t *bsp_gnss_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_gnss_open(bsp_gnss_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_gnss_close(bsp_gnss_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return ESP_OK;
}

esp_err_t bsp_gnss_read(bsp_gnss_handle_t handle, uint8_t *buf, size_t len, size_t *out_len, uint32_t timeout_ms)
{
    (void)buf;
    (void)len;
    (void)timeout_ms;
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_len != NULL, ESP_ERR_INVALID_ARG, TAG, "out_len is null");
    *out_len = 0;
    return ESP_ERR_NOT_SUPPORTED;
}

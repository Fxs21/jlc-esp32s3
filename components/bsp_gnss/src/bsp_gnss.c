#include "bsp_gnss.h"

#include <stdlib.h>

#include "bsp_boarddb.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"

#define BSP_GNSS_TAG "bsp_gnss"

struct bsp_gnss_s {
    const bsp_boarddb_gnss_desc_t *cfg;
};

static bsp_gnss_desc_t gnss_desc;

static const bsp_boarddb_gnss_desc_t *gnss_cfg(void)
{
    return &bsp_boarddb_get()->gnss;
}

const bsp_gnss_desc_t *bsp_gnss_get_desc(void)
{
    const bsp_boarddb_gnss_desc_t *cfg = gnss_cfg();
    gnss_desc.present = cfg->present && cfg->kind != BSP_BOARDDB_GNSS_KIND_NONE;
    return &gnss_desc;
}

esp_err_t bsp_gnss_open(bsp_gnss_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, BSP_GNSS_TAG, "out_handle is null");

    const bsp_boarddb_gnss_desc_t *cfg = gnss_cfg();
    ESP_RETURN_ON_FALSE(cfg->present, ESP_ERR_NOT_SUPPORTED, BSP_GNSS_TAG, "gnss not present");
    ESP_RETURN_ON_FALSE(cfg->kind == BSP_BOARDDB_GNSS_KIND_ATGM336H,
                        ESP_ERR_NOT_SUPPORTED, BSP_GNSS_TAG, "gnss kind not supported");

    struct bsp_gnss_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, BSP_GNSS_TAG, "no memory");
    handle->cfg = cfg;

    uart_config_t uart_cfg = {
        .baud_rate = cfg->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t ret = uart_param_config(cfg->uart_port, &uart_cfg);
    if (ret == ESP_OK) {
        ret = uart_set_pin(cfg->uart_port, cfg->tx_pin, cfg->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (ret == ESP_OK) {
        ret = uart_driver_install(cfg->uart_port, cfg->rx_buffer_size, 0, 0, NULL, 0);
    }
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_gnss_close(bsp_gnss_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_GNSS_TAG, "handle is null");
    esp_err_t ret = uart_driver_delete(handle->cfg->uart_port);
    free(handle);
    return ret;
}

esp_err_t bsp_gnss_read(bsp_gnss_handle_t handle, uint8_t *buf, size_t len, size_t *out_len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, BSP_GNSS_TAG, "handle is null");
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_INVALID_ARG, BSP_GNSS_TAG, "buf is null");
    ESP_RETURN_ON_FALSE(out_len != NULL, ESP_ERR_INVALID_ARG, BSP_GNSS_TAG, "out_len is null");

    int bytes = uart_read_bytes(handle->cfg->uart_port, buf, len, pdMS_TO_TICKS(timeout_ms));
    if (bytes < 0) {
        *out_len = 0;
        return ESP_FAIL;
    }
    *out_len = (size_t)bytes;
    return ESP_OK;
}

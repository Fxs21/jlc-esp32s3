#include "bsp_gnss.h"

#include <stdlib.h>

#include "doers3_pins.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"

#define TAG "bsp_gnss"

struct bsp_gnss_s {
    uart_port_t uart_port;
};

static const bsp_gnss_desc_t s_desc = {
    .present = true,
};

const bsp_gnss_desc_t *bsp_gnss_get_desc(void)
{
    return &s_desc;
}

esp_err_t bsp_gnss_open(bsp_gnss_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    struct bsp_gnss_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->uart_port = DOERS3_GNSS_UART;

    uart_config_t uart_cfg = {
        .baud_rate = DOERS3_GNSS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t ret = uart_param_config(handle->uart_port, &uart_cfg);
    if (ret == ESP_OK) {
        ret = uart_set_pin(handle->uart_port, DOERS3_GNSS_TX, DOERS3_GNSS_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (ret == ESP_OK) {
        ret = uart_driver_install(handle->uart_port, DOERS3_GNSS_RX_BUFFER_SIZE, 0, 0, NULL, 0);
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
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t ret = uart_driver_delete(handle->uart_port);
    free(handle);
    return ret;
}

esp_err_t bsp_gnss_read(bsp_gnss_handle_t handle, uint8_t *buf, size_t len, size_t *out_len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_INVALID_ARG, TAG, "buf is null");
    ESP_RETURN_ON_FALSE(out_len != NULL, ESP_ERR_INVALID_ARG, TAG, "out_len is null");

    int bytes = uart_read_bytes(handle->uart_port, buf, len, pdMS_TO_TICKS(timeout_ms));
    if (bytes < 0) {
        *out_len = 0;
        return ESP_FAIL;
    }
    *out_len = (size_t)bytes;
    return ESP_OK;
}

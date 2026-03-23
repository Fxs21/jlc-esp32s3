#include "bsp_gnss.h"

#include <stdlib.h>

#include "auras3_ioexp.h"
#include "auras3_pins.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"

#define TAG "bsp_gnss"

struct bsp_gnss_s {
    uart_port_t uart_port;
    bool ioexp_acquired;
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
    *out_handle = NULL;

    struct bsp_gnss_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    handle->uart_port = AURAS3_GNSS_UART;

    esp_err_t ret = auras3_ioexp_acquire();
    if (ret != ESP_OK) {
        goto err;
    }
    handle->ioexp_acquired = true;
    ESP_GOTO_ON_ERROR(auras3_ioexp_set_gps_reset(false), err, TAG, "release GNSS reset failed");

    uart_config_t uart_cfg = {
        .baud_rate = AURAS3_GNSS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_GOTO_ON_ERROR(uart_param_config(handle->uart_port, &uart_cfg), err, TAG, "config GNSS UART failed");
    ESP_GOTO_ON_ERROR(uart_set_pin(handle->uart_port,
                                   AURAS3_GNSS_TX,
                                   AURAS3_GNSS_RX,
                                   UART_PIN_NO_CHANGE,
                                   UART_PIN_NO_CHANGE),
                      err,
                      TAG,
                      "set GNSS UART pins failed");
    ESP_GOTO_ON_ERROR(uart_driver_install(handle->uart_port, AURAS3_GNSS_RX_BUFFER_SIZE, 0, 0, NULL, 0),
                      err,
                      TAG,
                      "install GNSS UART failed");

    *out_handle = handle;
    return ESP_OK;

err:
    if (handle != NULL) {
        if (handle->ioexp_acquired) {
            (void)auras3_ioexp_release();
        }
        free(handle);
    }
    return ret;
}

esp_err_t bsp_gnss_close(bsp_gnss_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    esp_err_t first_err = uart_driver_delete(handle->uart_port);
    if (handle->ioexp_acquired) {
        esp_err_t ret = auras3_ioexp_release();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    free(handle);
    return first_err;
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

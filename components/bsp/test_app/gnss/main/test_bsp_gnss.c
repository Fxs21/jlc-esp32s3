#include <ctype.h>
#include <stdio.h>

#include "bsp_gnss.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "test_bsp_gnss"
#define READ_TIMEOUT_MS 1000
#define READ_COUNT      20

static void print_nmea(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        putchar(isprint(buf[i]) || buf[i] == '\r' || buf[i] == '\n' ? buf[i] : '.');
    }
    fflush(stdout);
}

void app_main(void)
{
    if (!bsp_gnss_get_desc()->present) {
        ESP_LOGW(TAG, "gnss not present on this board");
        return;
    }

    bsp_gnss_handle_t gnss = NULL;
    ESP_ERROR_CHECK(bsp_gnss_open(&gnss));

    uint8_t buf[128] = {0};
    for (int i = 0; i < READ_COUNT; ++i) {
        size_t out_len = 0;
        esp_err_t ret = bsp_gnss_read(gnss, buf, sizeof(buf), &out_len, READ_TIMEOUT_MS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(ret));
            break;
        }
        if (out_len == 0) {
            ESP_LOGI(TAG, "no data");
        } else {
            ESP_LOGI(TAG, "read %u bytes", (unsigned)out_len);
            print_nmea(buf, out_len);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_ERROR_CHECK(bsp_gnss_close(gnss));
}

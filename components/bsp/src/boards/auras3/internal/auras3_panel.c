#include "auras3_panel.h"

#include <string.h>

#include "auras3_pins.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lock.h"

#define TAG "auras3_panel"
#define PANEL_BITS_PER_PIXEL 16
#define PANEL_TRANS_QUEUE_DEPTH 2
#define PANEL_DEFAULT_BRIGHTNESS 0

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    uint32_t ref_count;
    uint8_t brightness_percent;
    bool spi_inited;
} auras3_panel_state_t;

static auras3_panel_state_t s_panel;
static StaticSemaphore_t s_lock_buf;
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_panel_io_transfer_done_buf;
static SemaphoreHandle_t s_panel_io_transfer_done;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_lcd_panel_io_color_trans_done_cb_t s_panel_io_transfer_done_cb;
static void *s_panel_io_transfer_done_user_ctx;

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    (void)user_ctx;

    BaseType_t high_task_woken = pdFALSE;
    if (s_panel_io_transfer_done != NULL) {
        (void)xSemaphoreGiveFromISR(s_panel_io_transfer_done, &high_task_woken);
    }
    bool cb_woke = false;
    if (s_panel_io_transfer_done_cb != NULL) {
        cb_woke = s_panel_io_transfer_done_cb(panel_io, edata, s_panel_io_transfer_done_user_ctx);
    }
    return cb_woke || high_task_woken == pdTRUE;
}

static const co5300_lcd_init_cmd_t s_panel_init_cmds[] = {
    {0xFE, (uint8_t[]){0x00}, 1, 0},                    // Return to default command page.
    {0xC4, (uint8_t[]){0x80}, 1, 0},                    // Configure CO5300 vendor display mode.
    {0x3A, (uint8_t[]){0x55}, 1, 0},                    // Set RGB565 pixel format.
    {0x35, (uint8_t[]){0x00}, 1, 0},                    // Enable TE output signal.
    {0x53, (uint8_t[]){0x20}, 1, 0},                    // Enable display brightness control.
    {0x51, (uint8_t[]){0x00}, 1, 0},                    // Keep brightness off until the UI sets it.
    {0x63, (uint8_t[]){0xFF}, 1, 0},                    // Set vendor brightness/current limit to maximum.
    {0x2A, (uint8_t[]){0x00, 0x06, 0x01, 0xD7}, 4, 0}, // Set column address range with panel X offset.
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Set row address range.
    {0x11, NULL, 0, 60},                               // Exit sleep mode and wait for power stabilization.
    {0x29, NULL, 0, 0},                                // Turn display output on.
};

#define LOCK_TIMEOUT_MS UINT32_MAX

static esp_err_t ensure_panel_io_transfer_done_sem(void)
{
    if (s_panel_io_transfer_done != NULL) {
        return ESP_OK;
    }
    taskENTER_CRITICAL(&s_lock_mux);
    if (s_panel_io_transfer_done == NULL) {
        s_panel_io_transfer_done = xSemaphoreCreateBinaryStatic(&s_panel_io_transfer_done_buf);
    }
    taskEXIT_CRITICAL(&s_lock_mux);
    ESP_RETURN_ON_FALSE(s_panel_io_transfer_done != NULL, ESP_ERR_NO_MEM, TAG, "create panel IO transfer semaphore failed");
    return ESP_OK;
}

static esp_err_t lock(void)
{
    return bsp_lock_take(&s_lock, &s_lock_buf, &s_lock_mux, TAG, LOCK_TIMEOUT_MS);
}

static void unlock(void)
{
    bsp_lock_give(s_lock);
}

static esp_err_t panel_init(void)
{
    ESP_RETURN_ON_ERROR(ensure_panel_io_transfer_done_sem(), TAG, "panel IO transfer semaphore not ready");

    const size_t max_transfer_sz = AURAS3_LCD_WIDTH * AURAS3_LCD_HEIGHT * sizeof(uint16_t);
    const spi_bus_config_t bus_cfg = CO5300_PANEL_BUS_QSPI_CONFIG(AURAS3_LCD_PCLK,
                                                                  AURAS3_LCD_DATA0,
                                                                  AURAS3_LCD_DATA1,
                                                                  AURAS3_LCD_DATA2,
                                                                  AURAS3_LCD_DATA3,
                                                                  max_transfer_sz);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(AURAS3_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "init SPI bus failed");
    s_panel.spi_inited = true;

    esp_lcd_panel_io_spi_config_t io_cfg = CO5300_PANEL_IO_QSPI_CONFIG(AURAS3_LCD_CS, on_color_trans_done, NULL);
    io_cfg.trans_queue_depth = PANEL_TRANS_QUEUE_DEPTH;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)AURAS3_LCD_SPI_HOST, &io_cfg, &s_panel.io),
                        TAG,
                        "create panel IO failed");

    co5300_vendor_config_t vendor_cfg = {
        .init_cmds = s_panel_init_cmds,
        .init_cmds_size = sizeof(s_panel_init_cmds) / sizeof(s_panel_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = AURAS3_LCD_RESET,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = PANEL_BITS_PER_PIXEL,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_co5300(s_panel.io, &panel_cfg, &s_panel.panel), TAG, "create panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel.panel, AURAS3_LCD_X_GAP, AURAS3_LCD_Y_GAP), TAG, "set gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel.panel), TAG, "reset panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel.panel), TAG, "init panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel.panel, true), TAG, "turn display on failed");
    s_panel.brightness_percent = PANEL_DEFAULT_BRIGHTNESS;
    return ESP_OK;
}

static esp_err_t panel_deinit(void)
{
    esp_err_t first_err = ESP_OK;
    if (s_panel.panel != NULL) {
        esp_err_t ret = esp_lcd_panel_del(s_panel.panel);
        s_panel.panel = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (s_panel.io != NULL) {
        esp_err_t ret = esp_lcd_panel_io_del(s_panel.io);
        s_panel.io = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (s_panel.spi_inited) {
        esp_err_t ret = spi_bus_free(AURAS3_LCD_SPI_HOST);
        s_panel.spi_inited = false;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    memset(&s_panel, 0, sizeof(s_panel));
    return first_err;
}

esp_err_t auras3_panel_acquire(esp_lcd_panel_handle_t *out_panel, esp_lcd_panel_io_handle_t *out_io)
{
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_panel.ref_count == 0) {
        ret = panel_init();
        if (ret != ESP_OK) {
            (void)panel_deinit();
            goto out;
        }
    }
    s_panel.ref_count++;
    if (out_panel != NULL) {
        *out_panel = s_panel.panel;
    }
    if (out_io != NULL) {
        *out_io = s_panel.io;
    }

out:
    unlock();
    return ret;
}

esp_err_t auras3_panel_release(void)
{
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_panel.ref_count == 0) {
        ret = ESP_ERR_INVALID_STATE;
    } else if (s_panel.ref_count > 1) {
        s_panel.ref_count--;
        ret = ESP_OK;
    } else {
        ret = panel_deinit();
    }

    unlock();
    return ret;
}

esp_err_t auras3_panel_set_transfer_done_cb(esp_lcd_panel_io_color_trans_done_cb_t cb, void *user_ctx)
{
    s_panel_io_transfer_done_cb = cb;
    s_panel_io_transfer_done_user_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t auras3_panel_prepare_transfer(void)
{
    ESP_RETURN_ON_ERROR(ensure_panel_io_transfer_done_sem(), TAG, "panel IO transfer semaphore not ready");
    while (xSemaphoreTake(s_panel_io_transfer_done, 0) == pdTRUE) {
    }
    return ESP_OK;
}

esp_err_t auras3_panel_wait_transfer_done(void)
{
    ESP_RETURN_ON_ERROR(ensure_panel_io_transfer_done_sem(), TAG, "panel IO transfer semaphore not ready");
    return xSemaphoreTake(s_panel_io_transfer_done, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t auras3_panel_set_brightness(uint8_t percent)
{
    ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG, "percent must be 0..100");
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_panel.ref_count == 0 || s_panel.panel == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ret = esp_lcd_panel_co5300_set_brightness(s_panel.panel, percent);
        if (ret == ESP_OK) {
            s_panel.brightness_percent = percent;
        }
    }

    unlock();
    return ret;
}

esp_err_t auras3_panel_get_brightness(uint8_t *out_percent)
{
    ESP_RETURN_ON_FALSE(out_percent != NULL, ESP_ERR_INVALID_ARG, TAG, "out_percent is null");
    esp_err_t ret = lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_panel.ref_count == 0 || s_panel.panel == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        *out_percent = s_panel.brightness_percent;
        ret = ESP_OK;
    }

    unlock();
    return ret;
}

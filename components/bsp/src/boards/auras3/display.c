#include "bsp_display.h"
#include "bsp_display_internal.h"
#include "bsp_backlight.h"

#include <stdlib.h>
#include <string.h>

#include "auras3_pins.h"
#include "bsp_lvgl_port.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "bsp_display"

#define DISPLAY_BITS_PER_PIXEL    16
#define DISPLAY_BYTES_PER_PIXEL   2
#define PANEL_TRANS_QUEUE_DEPTH   10
#define PANEL_DEFAULT_BRIGHTNESS  0
#define LOCK_TIMEOUT_MS           UINT32_MAX

// Shared panel resource. backlight operates on the same CO5300 panel.
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static uint32_t s_ref_count;

// Guard s_ref_count + s_panel + s_panel_io against concurrent open/close
static StaticSemaphore_t s_lock_buf;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t panel_lock(void)
{
    if (s_lock == NULL) {
        taskENTER_CRITICAL(&s_lock_mux);
        if (s_lock == NULL) {
            s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf);
        }
        taskEXIT_CRITICAL(&s_lock_mux);
        if (s_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void panel_unlock(void)
{
    (void)xSemaphoreGive(s_lock);
}

// ------------------------------------------------------- panel init / deinit

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    bsp_lvgl_port_t *port = (bsp_lvgl_port_t *)user_ctx;
    if (port == NULL || port->transfer_done_cb == NULL) {
        return false;
    }
    return port->transfer_done_cb(port->transfer_done_user_ctx);
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

static esp_err_t panel_init(bsp_lvgl_port_t *port)
{
    const size_t max_transfer_sz = AURAS3_LCD_WIDTH * AURAS3_LCD_HEIGHT * sizeof(uint16_t);
    const spi_bus_config_t bus_cfg = CO5300_PANEL_BUS_QSPI_CONFIG(AURAS3_LCD_PCLK,
                                                                  AURAS3_LCD_DATA0,
                                                                  AURAS3_LCD_DATA1,
                                                                  AURAS3_LCD_DATA2,
                                                                  AURAS3_LCD_DATA3,
                                                                  max_transfer_sz);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(AURAS3_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO),
                        TAG, "init SPI bus failed");

    esp_lcd_panel_io_spi_config_t io_cfg = CO5300_PANEL_IO_QSPI_CONFIG(AURAS3_LCD_CS,
                                                                        on_color_trans_done,
                                                                        port);
    io_cfg.trans_queue_depth = PANEL_TRANS_QUEUE_DEPTH;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)AURAS3_LCD_SPI_HOST,
                                                  &io_cfg, &s_panel_io),
                        TAG, "create panel IO failed");

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
        .bits_per_pixel = DISPLAY_BITS_PER_PIXEL,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_co5300(s_panel_io, &panel_cfg, &s_panel),
                        TAG, "create panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, AURAS3_LCD_X_GAP, AURAS3_LCD_Y_GAP),
                        TAG, "set gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "turn display on failed");
    return ESP_OK;
}

static esp_err_t panel_deinit(void)
{
    esp_err_t first_err = ESP_OK;
    if (s_panel != NULL) {
        esp_err_t ret = esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (s_panel_io != NULL) {
        esp_err_t ret = esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    esp_err_t ret = spi_bus_free(AURAS3_LCD_SPI_HOST);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }
    return first_err;
}

// ------------------------------------------------------- panel brightness

static esp_err_t panel_set_brightness(uint8_t percent)
{
    ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG, "percent must be 0..100");
    esp_err_t ret = panel_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_panel == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ret = esp_lcd_panel_co5300_set_brightness(s_panel, percent);
    }

    panel_unlock();
    return ret;
}

// ------------------------------------------------------- backlight API

struct bsp_backlight_s {
    uint8_t percent;
};

static const bsp_backlight_desc_t s_bl_desc = {
    .present = true,
};

const bsp_backlight_desc_t *bsp_backlight_get_desc(void)
{
    return &s_bl_desc;
}

esp_err_t bsp_backlight_open(bsp_backlight_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    *out_handle = NULL;

    struct bsp_backlight_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    // Panel must be opened by display first
    esp_err_t ret = panel_lock();
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    if (s_panel == NULL) {
        panel_unlock();
        free(handle);
        return ESP_ERR_INVALID_STATE;
    }
    s_ref_count++;
    handle->percent = PANEL_DEFAULT_BRIGHTNESS;
    panel_unlock();

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_backlight_close(bsp_backlight_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");

    esp_err_t ret = panel_lock();
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }
    if (s_ref_count > 0) {
        s_ref_count--;
    }
    panel_unlock();

    free(handle);
    return ESP_OK;
}

esp_err_t bsp_backlight_set_percent(bsp_backlight_handle_t handle, uint8_t percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    if (percent > 100) {
        percent = 100;
    }
    ESP_RETURN_ON_ERROR(panel_set_brightness(percent), TAG, "set brightness failed");
    handle->percent = percent;
    return ESP_OK;
}

esp_err_t bsp_backlight_get_percent(bsp_backlight_handle_t handle, uint8_t *out_percent)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_percent != NULL, ESP_ERR_INVALID_ARG, TAG, "out_percent is null");
    *out_percent = handle->percent;
    return ESP_OK;
}

// ------------------------------------------------------- display API

struct bsp_display_s {
    bsp_lvgl_port_t port;
};

static const bsp_display_info_t s_info = {
    .present = true,
    .width = AURAS3_LCD_WIDTH,
    .height = AURAS3_LCD_HEIGHT,
    .bits_per_pixel = DISPLAY_BITS_PER_PIXEL,
};

static bool s_display_open;

// ------------------------------------------------------- LVGL callbacks

static bool lvgl_flush_ready_cb(void *user_ctx)
{
    lv_display_t *display = (lv_display_t *)user_ctx;
    if (display == NULL) {
        return false;
    }
    lv_display_flush_ready(display);
    return false;
}

#if LVGL_VERSION_MAJOR >= 9
static void rounder_event_cb(lv_event_t *event)
{
    lv_area_t *area = lv_event_get_invalidated_area(event);
    if (area == NULL) {
        return;
    }

    // CO5300 QSPI writes are restricted to the panel's 2-pixel granularity.
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}
#endif

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    struct bsp_display_s *handle = (struct bsp_display_s *)lv_display_get_user_data(display);
    if (handle == NULL) {
        lv_display_flush_ready(display);
        return;
    }

    const uint16_t width = area->x2 - area->x1 + 1;
    const uint16_t height = area->y2 - area->y1 + 1;
    const size_t tx_size = (size_t)width * height * DISPLAY_BYTES_PER_PIXEL;

    // AuraS3 native transfers are high-byte-first RGB565; LVGL buffers are host-endian RGB565.
    lv_draw_sw_rgb565_swap(px_map, (uint32_t)width * height);
    esp_err_t ret = bsp_display_write(handle, area->x1, area->y1, width, height, px_map, tx_size);

    if (ret != ESP_OK) {
        lv_display_flush_ready(display);
        ESP_LOGW(TAG, "LVGL flush failed: area=%d,%d %ux%u err=%s",
                 area->x1, area->y1, width, height, esp_err_to_name(ret));
    }
}

// ------------------------------------------------------- public display API

static esp_err_t validate_write_args(bsp_display_handle_t handle,
                                     uint16_t x,
                                     uint16_t y,
                                     uint16_t width,
                                     uint16_t height,
                                     const void *data,
                                     size_t data_size)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is null");
    ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "empty area");
    ESP_RETURN_ON_FALSE(x < AURAS3_LCD_WIDTH && y < AURAS3_LCD_HEIGHT,
                        ESP_ERR_INVALID_ARG, TAG, "origin out of range");
    ESP_RETURN_ON_FALSE((uint32_t)x + width <= AURAS3_LCD_WIDTH
                        && (uint32_t)y + height <= AURAS3_LCD_HEIGHT,
                        ESP_ERR_INVALID_ARG, TAG, "area out of range");
    size_t required = (size_t)width * height * DISPLAY_BYTES_PER_PIXEL;
    if (data_size < required) {
        ESP_LOGW(TAG, "display data too small: need=%u got=%u",
                 (unsigned)required, (unsigned)data_size);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(!s_display_open, ESP_ERR_INVALID_STATE, TAG, "display already open");
    *out_handle = NULL;

    struct bsp_display_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    // Backlight open is valid only after display open, so panel_init runs here
    esp_err_t ret = panel_lock();
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    if (s_ref_count == 0) {
        ret = panel_init(&handle->port);
        if (ret != ESP_OK) {
            (void)panel_deinit();
            panel_unlock();
            free(handle);
            return ret;
        }
    }
    s_ref_count++;
    s_display_open = true;
    panel_unlock();

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_display_close(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->port.lv_display == NULL, ESP_ERR_INVALID_STATE,
                        TAG, "LVGL port still open");

    esp_err_t ret = panel_lock();
    if (ret != ESP_OK) {
        free(handle);
        s_display_open = false;
        return ret;
    }

    if (s_ref_count == 1) {
        ret = panel_deinit();
    } else if (s_ref_count > 1) {
        s_ref_count--;
    } else {
        ret = ESP_ERR_INVALID_STATE;
    }

    s_display_open = false;
    panel_unlock();
    free(handle);
    return ret;
}

const bsp_display_info_t *bsp_display_get_info(bsp_display_handle_t handle)
{
    (void)handle;
    return &s_info;
}

esp_err_t bsp_display_set_done_cb(bsp_display_handle_t handle,
                                  bsp_display_transfer_done_cb_t cb,
                                  void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    handle->port.transfer_done_cb = cb;
    handle->port.transfer_done_user_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t bsp_display_write(bsp_display_handle_t handle,
                            uint16_t x,
                            uint16_t y,
                            uint16_t width,
                            uint16_t height,
                            const void *data,
                            size_t data_size)
{
    ESP_RETURN_ON_ERROR(validate_write_args(handle, x, y, width, height, data, data_size),
                        TAG, "invalid write args");
    return esp_lcd_panel_draw_bitmap(s_panel, x, y, x + width, y + height, data);
}

// ------------------------------------------------------- internal LVGL port API

esp_err_t bsp_display_port_lvgl_open(bsp_display_handle_t handle, lv_display_t **out_display)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_display != NULL, ESP_ERR_INVALID_ARG, TAG, "out_display is null");
    *out_display = NULL;

    ESP_RETURN_ON_ERROR(bsp_lvgl_port_open(&handle->port, AURAS3_LCD_WIDTH, AURAS3_LCD_HEIGHT, handle),
                        TAG, "lvgl port open failed");
    ESP_LOGI(TAG,
             "screen: panel=CO5300 %ux%u RGB565 QSPI gap=%u,%u lvgl=PARTIAL swap=on te_gpio=%d te_wait=off",
             AURAS3_LCD_WIDTH,
             AURAS3_LCD_HEIGHT,
             AURAS3_LCD_X_GAP,
             AURAS3_LCD_Y_GAP,
             AURAS3_LCD_TE);
    lv_display_set_flush_cb(handle->port.lv_display, lvgl_flush_cb);
#if LVGL_VERSION_MAJOR >= 9
    lv_display_add_event_cb(handle->port.lv_display, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
#endif
    ESP_RETURN_ON_ERROR(bsp_display_set_done_cb(handle, lvgl_flush_ready_cb, handle->port.lv_display),
                        TAG, "set transfer callback failed");

    *out_display = handle->port.lv_display;
    return ESP_OK;
}

esp_err_t bsp_display_port_lvgl_close(bsp_display_handle_t handle, lv_display_t *display)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");

    (void)bsp_display_set_done_cb(handle, NULL, NULL);
    return bsp_lvgl_port_close(&handle->port, display);
}

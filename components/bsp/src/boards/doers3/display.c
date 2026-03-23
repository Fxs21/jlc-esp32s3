#include "bsp_display.h"
#include "bsp_display_internal.h"

#include <stdlib.h>

#include "bsp_lvgl_port.h"
#include "doers3_ioexp.h"
#include "doers3_pins.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "bsp_display"
#define DISPLAY_BITS_PER_PIXEL     16
#define DISPLAY_BYTES_PER_PIXEL    2
#define DISPLAY_PIXEL_CLOCK_HZ     (80 * 1000 * 1000)
#define DISPLAY_CMD_BITS           8
#define DISPLAY_PARAM_BITS         8
#define DISPLAY_SPI_MODE           2
#define DISPLAY_TRANS_QUEUE_DEPTH  10

struct bsp_display_s {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
    bsp_lvgl_port_t port;
    bool spi_inited;
    bool ioexp_acquired;
};

static const bsp_display_info_t s_info = {
    .present = true,
    .width = DOERS3_LCD_WIDTH,
    .height = DOERS3_LCD_HEIGHT,
    .bits_per_pixel = DISPLAY_BITS_PER_PIXEL,
};

static bool s_display_open;

static bool lvgl_flush_ready_cb(void *user_ctx)
{
    lv_display_t *display = (lv_display_t *)user_ctx;
    if (display == NULL) {
        return false;
    }
    lv_display_flush_ready(display);
    return false;
}

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
    ESP_RETURN_ON_FALSE(x < DOERS3_LCD_WIDTH && y < DOERS3_LCD_HEIGHT, ESP_ERR_INVALID_ARG, TAG, "origin out of range");
    ESP_RETURN_ON_FALSE((uint32_t)x + width <= DOERS3_LCD_WIDTH && (uint32_t)y + height <= DOERS3_LCD_HEIGHT,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "area out of range");
    size_t required = (size_t)width * height * DISPLAY_BYTES_PER_PIXEL;
    if (data_size < required) {
        ESP_LOGW(TAG, "display data too small: need=%u got=%u", (unsigned)required, (unsigned)data_size);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    struct bsp_display_s *handle = (struct bsp_display_s *)lv_display_get_user_data(display);
    if (handle == NULL) {
        lv_display_flush_ready(display);
        return;
    }

    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    // DoerS3 ST7789 is configured little-endian, matching LVGL's RGB565 buffer layout.
    esp_err_t ret = bsp_display_write(handle,
                                      area->x1,
                                      area->y1,
                                      width,
                                      height,
                                      px_map,
                                      (size_t)width * height * DISPLAY_BYTES_PER_PIXEL);
    if (ret != ESP_OK) {
        lv_display_flush_ready(display);
    }
}

static esp_err_t panel_init(struct bsp_display_s *handle)
{
    const size_t max_transfer_sz = DOERS3_LCD_WIDTH * DOERS3_LCD_HEIGHT * sizeof(uint16_t);
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num = DOERS3_LCD_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = DOERS3_LCD_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = max_transfer_sz,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(DOERS3_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO),
                        TAG, "init SPI bus failed");
    handle->spi_inited = true;

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = DOERS3_LCD_DC,
        .cs_gpio_num = GPIO_NUM_NC,
        .pclk_hz = DISPLAY_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = DISPLAY_CMD_BITS,
        .lcd_param_bits = DISPLAY_PARAM_BITS,
        .spi_mode = DISPLAY_SPI_MODE,
        .trans_queue_depth = DISPLAY_TRANS_QUEUE_DEPTH,
        .on_color_trans_done = bsp_lvgl_transfer_done_bridge,
        .user_ctx = &handle->port,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DOERS3_LCD_SPI_HOST,
                                                  &io_cfg, &handle->panel_io),
                        TAG, "create panel IO failed");

    const esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num = DOERS3_LCD_RESET,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = DISPLAY_BITS_PER_PIXEL,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(handle->panel_io, &panel_dev_cfg, &handle->panel),
                        TAG, "create panel failed");

    // Init sequence
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(handle->panel), TAG, "reset panel failed");
    ESP_RETURN_ON_ERROR(doers3_ioexp_set_lcd_cs(true), TAG, "select panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(handle->panel), TAG, "init panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(handle->panel, true), TAG, "invert color failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(handle->panel, true), TAG, "swap xy failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(handle->panel, true, false), TAG, "mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(handle->panel, true), TAG, "disp on failed");
    return ESP_OK;
}

static esp_err_t panel_deinit(struct bsp_display_s *handle)
{
    esp_err_t first_err = ESP_OK;
    if (handle->panel != NULL) {
        esp_err_t ret = esp_lcd_panel_del(handle->panel);
        handle->panel = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    // Deselect panel via IO expander
    esp_err_t ret = doers3_ioexp_set_lcd_cs(false);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }
    if (handle->panel_io != NULL) {
        ret = esp_lcd_panel_io_del(handle->panel_io);
        handle->panel_io = NULL;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    if (handle->spi_inited) {
        ret = spi_bus_free(DOERS3_LCD_SPI_HOST);
        handle->spi_inited = false;
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }
    return first_err;
}

// ---------- public display API ----------

esp_err_t bsp_display_open(bsp_display_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    ESP_RETURN_ON_FALSE(!s_display_open, ESP_ERR_INVALID_STATE, TAG, "display already open");
    *out_handle = NULL;

    struct bsp_display_s *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    esp_err_t ret = doers3_ioexp_acquire();
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }
    handle->ioexp_acquired = true;

    ret = panel_init(handle);
    if (ret != ESP_OK) {
        (void)bsp_display_close(handle);
        return ret;
    }

    s_display_open = true;
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t bsp_display_close(bsp_display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(handle->port.lv_display == NULL, ESP_ERR_INVALID_STATE, TAG, "LVGL port still open");

    esp_err_t first_err = panel_deinit(handle);

    if (handle->ioexp_acquired) {
        esp_err_t ret = doers3_ioexp_release();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }

    free(handle);
    s_display_open = false;
    return first_err;
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
    ESP_RETURN_ON_ERROR(validate_write_args(handle, x, y, width, height, data, data_size), TAG, "invalid write args");
    ESP_RETURN_ON_FALSE(handle->panel != NULL, ESP_ERR_INVALID_STATE, TAG, "panel is null");
    return esp_lcd_panel_draw_bitmap(handle->panel, x, y, x + width, y + height, data);
}

// ---------- internal LVGL port API ----------

esp_err_t bsp_display_port_lvgl_open(bsp_display_handle_t handle, lv_display_t **out_display)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(out_display != NULL, ESP_ERR_INVALID_ARG, TAG, "out_display is null");
    *out_display = NULL;

    ESP_RETURN_ON_ERROR(bsp_lvgl_port_open(&handle->port, DOERS3_LCD_WIDTH, DOERS3_LCD_HEIGHT, handle),
                        TAG, "lvgl port open failed");
    lv_display_set_flush_cb(handle->port.lv_display, lvgl_flush_cb);
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

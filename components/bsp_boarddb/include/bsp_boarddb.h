#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_camera.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_BOARDDB_DOERS3 = 0,
    BSP_BOARDDB_AURAS3,
} bsp_boarddb_id_t;

#define BSP_BOARDDB_CAP_IOEXP     (1ULL << 0)
#define BSP_BOARDDB_CAP_DISPLAY   (1ULL << 1)
#define BSP_BOARDDB_CAP_TOUCH     (1ULL << 2)
#define BSP_BOARDDB_CAP_BACKLIGHT (1ULL << 3)
#define BSP_BOARDDB_CAP_SDCARD    (1ULL << 4)
#define BSP_BOARDDB_CAP_IMU       (1ULL << 5)
#define BSP_BOARDDB_CAP_AUDIO     (1ULL << 6)
#define BSP_BOARDDB_CAP_CAMERA    (1ULL << 7)
#define BSP_BOARDDB_CAP_GNSS      (1ULL << 8)
#define BSP_BOARDDB_CAP_POWER     (1ULL << 9)

typedef enum {
    BSP_BOARDDB_CTRL_PIN_NONE = 0,
    BSP_BOARDDB_CTRL_PIN_GPIO,
    BSP_BOARDDB_CTRL_PIN_IOEXP,
} bsp_boarddb_ctrl_pin_type_t;

typedef struct {
    bsp_boarddb_ctrl_pin_type_t type;
    bool active_high;
    union {
        gpio_num_t gpio_num;
        uint8_t ioexp_num;
    } num;
} bsp_boarddb_ctrl_pin_t;

typedef struct {
    bool present;
    i2c_port_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    uint8_t glitch_ignore_cnt;
    bool enable_internal_pullup;
} bsp_boarddb_i2c_desc_t;

typedef enum {
    BSP_BOARDDB_IOEXP_KIND_NONE = 0,
    BSP_BOARDDB_IOEXP_KIND_PCA9557,
} bsp_boarddb_ioexp_kind_t;

typedef struct {
    bool present;
    bsp_boarddb_ioexp_kind_t kind;
    uint8_t address;
    uint32_t scl_speed_hz;
    uint8_t output_default;
    uint8_t direction_mask;
} bsp_boarddb_ioexp_desc_t;

typedef enum {
    BSP_BOARDDB_DISPLAY_KIND_NONE = 0,
    BSP_BOARDDB_DISPLAY_KIND_ST7789,
} bsp_boarddb_display_kind_t;

typedef enum {
    BSP_BOARDDB_TOUCH_KIND_NONE = 0,
    BSP_BOARDDB_TOUCH_KIND_FT5X06,
} bsp_boarddb_touch_kind_t;

typedef enum {
    BSP_BOARDDB_BACKLIGHT_KIND_NONE = 0,
    BSP_BOARDDB_BACKLIGHT_KIND_LEDC,
} bsp_boarddb_backlight_kind_t;

typedef struct {
    bool present;
    bsp_boarddb_display_kind_t kind;
    uint16_t width;
    uint16_t height;
    lcd_rgb_data_endian_t data_endian;
    bool invert_color;
    spi_host_device_t spi_host;
    bsp_boarddb_ctrl_pin_t lcd_cs;
    gpio_num_t lcd_dc;
    gpio_num_t lcd_mosi;
    gpio_num_t lcd_sclk;
    gpio_num_t panel_reset_gpio;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} bsp_boarddb_display_desc_t;

typedef struct {
    bool present;
    bsp_boarddb_touch_kind_t kind;
    uint8_t address;
    uint16_t x_max;
    uint16_t y_max;
    gpio_num_t reset_gpio;
    gpio_num_t int_gpio;
    uint8_t level_reset;
    uint8_t level_interrupt;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} bsp_boarddb_touch_desc_t;

typedef struct {
    bool present;
    bsp_boarddb_backlight_kind_t kind;
    gpio_num_t gpio;
    bool output_invert;
} bsp_boarddb_backlight_desc_t;

typedef enum {
    BSP_BOARDDB_SDCARD_KIND_NONE = 0,
    BSP_BOARDDB_SDCARD_KIND_SDMMC,
} bsp_boarddb_sdcard_kind_t;

typedef struct {
    bool present;
    bsp_boarddb_sdcard_kind_t kind;
    gpio_num_t clk;
    gpio_num_t cmd;
    gpio_num_t d0;
    gpio_num_t d1;
    gpio_num_t d2;
    gpio_num_t d3;
    gpio_num_t cd;
    gpio_num_t wp;
    uint8_t width;
    uint32_t slot_flags;
    int max_freq_khz;
    bool format_if_mount_failed;
    int max_files;
    bool disk_status_check_enable;
    bool use_one_fat;
} bsp_boarddb_sdcard_desc_t;

typedef struct {
    bool present;
    uint8_t address;
    bool accel_unit_mps2;
    bool gyro_unit_rads;
    uint8_t display_precision;
} bsp_boarddb_imu_desc_t;

typedef struct {
    bool present;
    bool has_playback;
    bool has_record;
    i2s_port_t i2s_port;
    gpio_num_t mclk;
    gpio_num_t bclk;
    gpio_num_t ws;
    gpio_num_t dout;
    gpio_num_t din;
    uint8_t es8311_addr;
    uint8_t es7210_addr;
    uint8_t es7210_mic_mask;
    bsp_boarddb_ctrl_pin_t pa_enable;
    bool use_mclk;
    uint16_t mclk_multiple;
} bsp_boarddb_audio_desc_t;

typedef struct {
    bool present;
    bsp_boarddb_ctrl_pin_t pwdn;
    gpio_num_t reset;
    gpio_num_t xclk;
    gpio_num_t d7;
    gpio_num_t d6;
    gpio_num_t d5;
    gpio_num_t d4;
    gpio_num_t d3;
    gpio_num_t d2;
    gpio_num_t d1;
    gpio_num_t d0;
    gpio_num_t vsync;
    gpio_num_t href;
    gpio_num_t pclk;
    uint32_t xclk_freq_hz;
    pixformat_t pixel_format;
    framesize_t frame_size;
    int jpeg_quality;
    int fb_count;
    camera_fb_location_t fb_location;
    camera_grab_mode_t grab_mode;
    bool enable_hmirror;
    bool enable_vflip;
} bsp_boarddb_camera_desc_t;

typedef enum {
    BSP_BOARDDB_GNSS_KIND_NONE = 0,
    BSP_BOARDDB_GNSS_KIND_ATGM336H,
} bsp_boarddb_gnss_kind_t;

typedef struct {
    bool present;
    bsp_boarddb_gnss_kind_t kind;
    uart_port_t uart_port;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    int baud_rate;
    int rx_buffer_size;
    uint32_t read_timeout_ms;
} bsp_boarddb_gnss_desc_t;

typedef enum {
    BSP_BOARDDB_POWER_KIND_NONE = 0,
    BSP_BOARDDB_POWER_KIND_AXP2101,
} bsp_boarddb_power_kind_t;

typedef struct {
    bool present;
    bsp_boarddb_power_kind_t kind;
    uint8_t address;
} bsp_boarddb_power_desc_t;

typedef struct {
    bsp_boarddb_id_t id;
    const char *name;
    uint64_t capabilities;
    bsp_boarddb_i2c_desc_t i2c;
    bsp_boarddb_ioexp_desc_t ioexp;
    bsp_boarddb_display_desc_t display;
    bsp_boarddb_touch_desc_t touch;
    bsp_boarddb_backlight_desc_t backlight;
    bsp_boarddb_sdcard_desc_t sdcard;
    bsp_boarddb_imu_desc_t imu;
    bsp_boarddb_audio_desc_t audio;
    bsp_boarddb_camera_desc_t camera;
    bsp_boarddb_gnss_desc_t gnss;
    bsp_boarddb_power_desc_t power;
} bsp_boarddb_desc_t;

const bsp_boarddb_desc_t *bsp_boarddb_get(void);

#ifdef __cplusplus
}
#endif

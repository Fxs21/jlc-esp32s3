#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsp_camera_gc0308.h"
#include "bsp_boarddb.h"
#include "bsp_i2c_bus.h"
#include "bsp_ioexp.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sensor.h"
#include "soc/soc_caps.h"

#define BSP_CAMERA_TAG "bsp_camera"

typedef enum {
    BSP_CAMERA_STATE_CREATED = 0,
    BSP_CAMERA_STATE_STARTED,
} bsp_camera_state_t;

typedef struct {
    const bsp_camera_gc0308_cfg_t *cfg;
    i2c_port_t sccb_i2c_port;
    bsp_camera_state_t state;
    bool bus_acquired;
    bool ioexp_acquired;
} bsp_camera_gc0308_ctx_t;

static bsp_camera_gc0308_ctx_t *camera;

static inline bsp_ioexp_pin_t to_ioexp_pin(bsp_camera_ctrl_pin_t pin)
{
    return (bsp_ioexp_pin_t) {
        .pin = pin.num.ioexp_num,
        .active_high = pin.active_high,
    };
}

static inline bool ctrl_pin_is_gpio(const bsp_camera_ctrl_pin_t *pin)
{
    return pin->type == BSP_CAMERA_CTRL_PIN_GPIO && pin->num.gpio_num != GPIO_NUM_NC;
}

static inline bool ctrl_pin_is_ioexp(const bsp_camera_ctrl_pin_t *pin)
{
    return pin->type == BSP_CAMERA_CTRL_PIN_IOEXP;
}

static int ctrl_pin_gpio_level(const bsp_camera_ctrl_pin_t *pin, bool asserted)
{
    return (asserted == pin->active_high) ? 1 : 0;
}

static esp_err_t ctrl_pin_init_output(const bsp_camera_ctrl_pin_t *pin, bool asserted)
{
    if (!ctrl_pin_is_gpio(pin)) {
        return ESP_OK;
    }

    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << pin->num.gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), BSP_CAMERA_TAG, "configure gpio pin failed");
    return gpio_set_level(pin->num.gpio_num, ctrl_pin_gpio_level(pin, asserted));
}

static esp_err_t ctrl_pin_set_level(const bsp_camera_ctrl_pin_t *pin, bool asserted)
{
    if (pin->type == BSP_CAMERA_CTRL_PIN_NONE) {
        return ESP_OK;
    }
    if (ctrl_pin_is_gpio(pin)) {
        return gpio_set_level(pin->num.gpio_num, ctrl_pin_gpio_level(pin, asserted));
    }
    if (ctrl_pin_is_ioexp(pin)) {
        return bsp_ioexp_set_level(to_ioexp_pin(*pin), asserted);
    }
    return ESP_ERR_INVALID_ARG;
}

static bool has_pwdn_control(const bsp_camera_gc0308_ctx_t *ctx)
{
    return ctx->cfg->pwdn.type != BSP_CAMERA_CTRL_PIN_NONE;
}

static esp_err_t set_pwdn(bsp_camera_gc0308_ctx_t *ctx, bool asserted)
{
    if (!has_pwdn_control(ctx)) {
        return ESP_OK;
    }
    if (ctrl_pin_is_ioexp(&ctx->cfg->pwdn)) {
        ESP_RETURN_ON_FALSE(ctx->ioexp_acquired, ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "ioexp required");
    }
    return ctrl_pin_set_level(&ctx->cfg->pwdn, asserted);
}

static camera_config_t make_config(const bsp_camera_gc0308_ctx_t *ctx)
{
    const bsp_boarddb_i2c_desc_t *i2c_cfg = &bsp_boarddb_get()->i2c;
    const camera_config_t camera_cfg = {
        .pin_pwdn = GPIO_NUM_NC,
        .pin_reset = ctx->cfg->reset,
        .pin_xclk = ctx->cfg->xclk,
        .pin_sccb_sda = i2c_cfg->sda,
        .pin_sccb_scl = i2c_cfg->scl,
        .pin_d7 = ctx->cfg->d7,
        .pin_d6 = ctx->cfg->d6,
        .pin_d5 = ctx->cfg->d5,
        .pin_d4 = ctx->cfg->d4,
        .pin_d3 = ctx->cfg->d3,
        .pin_d2 = ctx->cfg->d2,
        .pin_d1 = ctx->cfg->d1,
        .pin_d0 = ctx->cfg->d0,
        .pin_vsync = ctx->cfg->vsync,
        .pin_href = ctx->cfg->href,
        .pin_pclk = ctx->cfg->pclk,
        .xclk_freq_hz = ctx->cfg->xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_1,
        .ledc_channel = LEDC_CHANNEL_1,
        .pixel_format = ctx->cfg->pixel_format,
        .frame_size = ctx->cfg->frame_size,
        .jpeg_quality = ctx->cfg->jpeg_quality,
        .fb_count = ctx->cfg->fb_count,
        .fb_location = ctx->cfg->fb_location,
        .grab_mode = ctx->cfg->grab_mode,
        .sccb_i2c_port = ctx->sccb_i2c_port,
    };
    return camera_cfg;
}

static esp_err_t apply_sensor_cfg(bsp_camera_gc0308_ctx_t *ctx)
{
    sensor_t *sensor = esp_camera_sensor_get();
    ESP_RETURN_ON_FALSE(sensor != NULL, ESP_ERR_NOT_FOUND, BSP_CAMERA_TAG, "camera sensor not found");

    if (sensor->set_hmirror != NULL) {
        ESP_RETURN_ON_FALSE(sensor->set_hmirror(sensor, ctx->cfg->enable_hmirror) == 0,
                            ESP_FAIL, BSP_CAMERA_TAG, "set_hmirror failed");
    }
    if (sensor->set_vflip != NULL) {
        ESP_RETURN_ON_FALSE(sensor->set_vflip(sensor, ctx->cfg->enable_vflip) == 0,
                            ESP_FAIL, BSP_CAMERA_TAG, "set_vflip failed");
    }
    return ESP_OK;
}

static esp_err_t init_hw(bsp_camera_gc0308_ctx_t *ctx)
{
    if (ctx->state == BSP_CAMERA_STATE_STARTED) {
        return ESP_OK;
    }

    bool powered_up = false;
    if (has_pwdn_control(ctx)) {
        ESP_RETURN_ON_ERROR(set_pwdn(ctx, false), BSP_CAMERA_TAG, "power up camera failed");
        powered_up = true;
    }

    camera_config_t config = make_config(ctx);
    esp_err_t ret = esp_camera_init(&config);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = apply_sensor_cfg(ctx);
    if (ret != ESP_OK) {
        (void)esp_camera_deinit();
        goto err;
    }

    ctx->state = BSP_CAMERA_STATE_STARTED;
    return ESP_OK;

err:
    if (powered_up && has_pwdn_control(ctx)) {
        esp_err_t pwdn_ret = set_pwdn(ctx, true);
        if (pwdn_ret != ESP_OK) {
            ESP_LOGW(BSP_CAMERA_TAG, "rollback camera pwdn failed: %s", esp_err_to_name(pwdn_ret));
        }
    }
    return ret;
}

static esp_err_t resolve_i2c_port(i2c_master_bus_handle_t i2c_bus, i2c_port_t *out_port)
{
    ESP_RETURN_ON_FALSE(i2c_bus != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "i2c_bus is null");
    ESP_RETURN_ON_FALSE(out_port != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "out_port is null");

    for (int port = 0; port < SOC_I2C_NUM; ++port) {
        i2c_master_bus_handle_t bus = NULL;
        if (i2c_master_get_bus_handle((i2c_port_num_t)port, &bus) == ESP_OK && bus == i2c_bus) {
            *out_port = (i2c_port_t)port;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t bsp_camera_gc0308_create(const void *config, void **out_ctx)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "config is null");
    ESP_RETURN_ON_FALSE(out_ctx != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "out_ctx is null");
    ESP_RETURN_ON_FALSE(camera == NULL, ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "camera instance already exists");

    const bsp_camera_gc0308_cfg_t *cfg = config;
    bsp_camera_gc0308_ctx_t *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_NO_MEM, BSP_CAMERA_TAG, "no memory");
    ctx->cfg = cfg;

    bsp_i2c_bus_cfg_t bus_cfg = bsp_i2c_bus_default_cfg();
    esp_err_t ret = bsp_i2c_bus_open(&bus_cfg);
    if (ret != ESP_OK) {
        free(ctx);
        return ret;
    }
    ctx->bus_acquired = true;

    i2c_master_bus_handle_t i2c_bus = NULL;
    ret = bsp_i2c_bus_get(&i2c_bus);
    if (ret != ESP_OK) {
        (void)bsp_camera_gc0308_destroy(ctx);
        return ret;
    }

    ret = resolve_i2c_port(i2c_bus, &ctx->sccb_i2c_port);
    if (ret != ESP_OK) {
        (void)bsp_camera_gc0308_destroy(ctx);
        return ret;
    }

    if (ctrl_pin_is_gpio(&cfg->pwdn)) {
        ret = ctrl_pin_init_output(&cfg->pwdn, true);
        if (ret != ESP_OK) {
            (void)bsp_camera_gc0308_destroy(ctx);
            return ret;
        }
    }

    if (ctrl_pin_is_ioexp(&cfg->pwdn)) {
        ret = bsp_ioexp_open();
        if (ret != ESP_OK) {
            (void)bsp_camera_gc0308_destroy(ctx);
            return ret;
        }
        ctx->ioexp_acquired = true;
    }

    ctx->state = BSP_CAMERA_STATE_CREATED;
    camera = ctx;
    *out_ctx = ctx;
    return ESP_OK;
}

esp_err_t bsp_camera_gc0308_start(void *ctx_ptr)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "ctx is null");

    bsp_camera_gc0308_ctx_t *ctx = ctx_ptr;
    if (ctx->state == BSP_CAMERA_STATE_STARTED) {
        return ESP_OK;
    }
    if (camera != ctx) {
        return ESP_ERR_INVALID_STATE;
    }

    return init_hw(ctx);
}

esp_err_t bsp_camera_gc0308_fb_get(void *ctx_ptr, camera_fb_t **out_frame)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "ctx is null");
    ESP_RETURN_ON_FALSE(out_frame != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "out_frame is null");

    bsp_camera_gc0308_ctx_t *ctx = ctx_ptr;
    ESP_RETURN_ON_FALSE(ctx->state == BSP_CAMERA_STATE_STARTED, ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "camera not started");

    camera_fb_t *frame = esp_camera_fb_get();
    ESP_RETURN_ON_FALSE(frame != NULL, ESP_ERR_NOT_FOUND, BSP_CAMERA_TAG, "esp_camera_fb_get returned null");

    *out_frame = frame;
    return ESP_OK;
}

esp_err_t bsp_camera_gc0308_fb_return(void *ctx_ptr, camera_fb_t *frame)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "ctx is null");
    ESP_RETURN_ON_FALSE(frame != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "frame is null");

    bsp_camera_gc0308_ctx_t *ctx = ctx_ptr;
    ESP_RETURN_ON_FALSE(ctx->state == BSP_CAMERA_STATE_STARTED, ESP_ERR_INVALID_STATE, BSP_CAMERA_TAG, "camera not started");

    esp_camera_fb_return(frame);
    return ESP_OK;
}

esp_err_t bsp_camera_gc0308_stop(void *ctx_ptr)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "ctx is null");

    bsp_camera_gc0308_ctx_t *ctx = ctx_ptr;
    if (ctx->state != BSP_CAMERA_STATE_STARTED) {
        return ESP_OK;
    }

    esp_err_t first_err = ESP_OK;
    esp_err_t ret = esp_camera_deinit();
    if (ret != ESP_OK) {
        first_err = ret;
        ESP_LOGE(BSP_CAMERA_TAG, "esp_camera_deinit failed: %s", esp_err_to_name(ret));
    } else {
        ctx->state = BSP_CAMERA_STATE_CREATED;
    }

    if (has_pwdn_control(ctx)) {
        ret = set_pwdn(ctx, true);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }

    return first_err;
}

sensor_t *bsp_camera_gc0308_sensor(void *ctx_ptr)
{
    if (ctx_ptr == NULL) {
        return NULL;
    }

    bsp_camera_gc0308_ctx_t *ctx = ctx_ptr;
    if (ctx->state != BSP_CAMERA_STATE_STARTED) {
        return NULL;
    }
    return esp_camera_sensor_get();
}

esp_err_t bsp_camera_gc0308_destroy(void *ctx_ptr)
{
    ESP_RETURN_ON_FALSE(ctx_ptr != NULL, ESP_ERR_INVALID_ARG, BSP_CAMERA_TAG, "ctx is null");

    bsp_camera_gc0308_ctx_t *ctx = ctx_ptr;
    esp_err_t first_err = bsp_camera_gc0308_stop(ctx);

    if (camera == ctx) {
        camera = NULL;
    }
    if (ctx->ioexp_acquired) {
        esp_err_t ret = bsp_ioexp_close();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
        ctx->ioexp_acquired = false;
    }
    if (ctx->bus_acquired) {
        esp_err_t ret = bsp_i2c_bus_close();
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
        ctx->bus_acquired = false;
    }

    ctx->state = BSP_CAMERA_STATE_CREATED;
    free(ctx);
    return first_err;
}

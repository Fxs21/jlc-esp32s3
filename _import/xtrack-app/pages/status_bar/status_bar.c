/**
 * @file status_bar.c
 * @brief Status bar — persistent top-layer overlay (X-Track faithful)
 *
 * X-Track features replicated:
 *   - Two background styles: transparent (map) / semi-opaque black (normal)
 *   - Battery fill bar with percentage height
 *   - SD card icon slides out with overshoot when unmounted
 *   - Appear animation with 1s delay
 *   - Satellite icon + count, centered clock
 */
#include "status_bar.h"
#include "hal/hal_clock.h"
#include "hal/hal_gps.h"
#include "hal/hal_power.h"
#include "resource/resource_pool.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

#define TAG "status_bar"
#define BAR_HEIGHT 25

/* Battery fill bar dimensions (inside battery icon) */
#define BATT_USAGE_H_OFFSET 6
#define BATT_USAGE_W_OFFSET 4

typedef struct {
    lv_obj_t *cont;

    /* Satellite (left) */
    lv_obj_t *img_sat;
    lv_obj_t *label_sat;

    /* SD card (left of center) */
    lv_obj_t *img_sd;

    /* Clock (center) */
    lv_obj_t *label_clock;

    /* Battery (right) */
    lv_obj_t *img_batt;
    lv_obj_t *obj_usage;  /**< White fill bar inside battery */
    lv_obj_t *label_batt;

    account_t *account;
    hal_clock_info_t clock;
    hal_power_info_t power;
    bool is_charging_anim_active;
} status_bar_ctx_t;

static status_bar_ctx_t s_ctx;

static void batt_anim_set_height(void *var, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)var, v);
}

static void batt_usage_set_opa(void *var, int32_t opa)
{
    lv_obj_set_style_opa((lv_obj_t *)var, opa, 0);
}

/* Forward declaration */
static void batt_charging_anim_start(lv_obj_t *obj);

static void batt_anim_opa_finish(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)lv_anim_get_user_data(a);
    batt_usage_set_opa(obj, LV_OPA_COVER);
    batt_charging_anim_start(obj);
}

static void batt_height_finish(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    lv_anim_t a_o;
    lv_anim_init(&a_o);
    lv_anim_set_var(&a_o, obj);
    lv_anim_set_exec_cb(&a_o, (lv_anim_exec_xcb_t)batt_usage_set_opa);
    lv_anim_set_user_data(&a_o, obj);
    lv_anim_set_ready_cb(&a_o, batt_anim_opa_finish);
    lv_anim_set_values(&a_o, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_early_apply(&a_o, true);
    lv_anim_set_delay(&a_o, 500);
    lv_anim_set_time(&a_o, 500);
    lv_anim_start(&a_o);
}

static void batt_charging_anim_start(lv_obj_t *obj)
{
    lv_anim_t a_h;
    lv_anim_init(&a_h);
    lv_anim_set_var(&a_h, obj);
    lv_anim_set_exec_cb(&a_h, batt_anim_set_height);
    lv_anim_set_user_data(&a_h, obj);
    lv_anim_set_values(&a_h, 0, lv_obj_get_style_height(s_ctx.img_batt, 0) - BATT_USAGE_H_OFFSET);
    lv_anim_set_time(&a_h, 1000);
    lv_anim_set_ready_cb(&a_h, batt_height_finish);
    lv_anim_start(&a_h);
}

static void refresh_display(void)
{
    lv_label_set_text_fmt(s_ctx.label_sat, "%d", s_ctx.power.percentage > 50 ? 8 : 0);
    lv_label_set_text_fmt(s_ctx.label_clock, "%02d:%02d",
                          s_ctx.clock.hour, s_ctx.clock.minute);

    lv_label_set_text_fmt(s_ctx.label_batt, "%d", s_ctx.power.percentage);

    lv_obj_clear_state(s_ctx.img_sd, LV_STATE_DISABLED);

    lv_coord_t h = lv_map(s_ctx.power.percentage, 0, 100, 0,
                          lv_obj_get_style_height(s_ctx.img_batt, 0) - BATT_USAGE_H_OFFSET);
    lv_obj_set_height(s_ctx.obj_usage, h);

    if (s_ctx.power.is_charging) {
        if (!s_ctx.is_charging_anim_active) {
            s_ctx.is_charging_anim_active = true;
            batt_charging_anim_start(s_ctx.obj_usage);
        }
    } else {
        if (s_ctx.is_charging_anim_active) {
            lv_anim_del(s_ctx.obj_usage, NULL);
            batt_usage_set_opa(s_ctx.obj_usage, LV_OPA_COVER);
            s_ctx.is_charging_anim_active = false;
        }
    }
}

static void on_timer(lv_timer_t *timer)
{
    (void)timer;

    hal_gps_info_t gps_info;
    hal_power_info_t power;

    if (account_pull(s_ctx.account, "GPS", &gps_info, sizeof(gps_info)) == ACCOUNT_OK)
        lv_label_set_text_fmt(s_ctx.label_sat, "%d", (int)gps_info.satellites);

    if (account_pull(s_ctx.account, "Power", &power, sizeof(power)) == ACCOUNT_OK) {
        s_ctx.power = power;

        lv_label_set_text_fmt(s_ctx.label_batt, "%d", power.percentage);

        lv_coord_t h = lv_map(power.percentage, 0, 100, 0,
                              lv_obj_get_style_height(s_ctx.img_batt, 0) - BATT_USAGE_H_OFFSET);
        lv_obj_set_height(s_ctx.obj_usage, h);

        if (power.is_charging) {
            if (!s_ctx.is_charging_anim_active) {
                s_ctx.is_charging_anim_active = true;
                batt_charging_anim_start(s_ctx.obj_usage);
            }
        } else {
            if (s_ctx.is_charging_anim_active) {
                lv_anim_del(s_ctx.obj_usage, NULL);
                batt_usage_set_opa(s_ctx.obj_usage, LV_OPA_COVER);
                s_ctx.is_charging_anim_active = false;
            }
        }
    }
}

static void on_hide_finish(lv_anim_t *a)
{
    lv_obj_add_flag((lv_obj_t *)lv_anim_get_user_data(a), LV_OBJ_FLAG_HIDDEN);
}

static int on_data_event(account_t *account, account_event_param_t *param)
{
    (void)account;

    /* Handle published data (Clock / Power) */
    if (param->event == ACCOUNT_EVENT_PUB_PUBLISH) {
        if (strcmp(param->tran->id, "Clock") == 0 && param->size == sizeof(hal_clock_info_t)) {
            memcpy(&s_ctx.clock, param->data, sizeof(hal_clock_info_t));
        } else if (strcmp(param->tran->id, "Power") == 0 && param->size == sizeof(hal_power_info_t)) {
            memcpy(&s_ctx.power, param->data, sizeof(hal_power_info_t));
        } else {
            return ACCOUNT_ERR_UNSUPPORTED;
        }
        refresh_display();
        return ACCOUNT_OK;
    }

    /* Handle Notify commands (APPEAR / SET_STYLE) */
    if (param->event == ACCOUNT_EVENT_NOTIFY) {
        if (param->size != sizeof(status_bar_info_t))
            return ACCOUNT_ERR_SIZE;

        status_bar_info_t *info = (status_bar_info_t *)param->data;

        switch (info->cmd) {
    case STATUS_BAR_CMD_APPEAR: {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_ctx.cont);
        lv_anim_set_values(&a, info->param.appear ? -BAR_HEIGHT : 0,
                            info->param.appear ? 0 : -BAR_HEIGHT);
        lv_anim_set_time(&a, 500);
        lv_anim_set_delay(&a, info->param.appear ? 1000 : 0);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_early_apply(&a, true);
        if (!info->param.appear) {
            lv_anim_set_user_data(&a, s_ctx.cont);
            lv_anim_set_ready_cb(&a, on_hide_finish);
        }
        if (info->param.appear)
            lv_obj_clear_flag(s_ctx.cont, LV_OBJ_FLAG_HIDDEN);
        lv_anim_start(&a);
        LOG_I(TAG, "StatusBar %s", info->param.appear ? "shown" : "hidden");
        return ACCOUNT_OK;
    }

    case STATUS_BAR_CMD_SET_STYLE:
        if (info->param.style == STATUS_BAR_STYLE_TRANSP)
            lv_obj_clear_state(s_ctx.cont, LV_STATE_USER_1);
        else
            lv_obj_add_state(s_ctx.cont, LV_STATE_USER_1);
        return ACCOUNT_OK;
    }

    return ACCOUNT_ERR_UNSUPPORTED;
    }

    return ACCOUNT_ERR_UNSUPPORTED;
}

void status_bar_init(data_center_t *dc)
{
    memset(&s_ctx, 0, sizeof(s_ctx));

    s_ctx.account = account_create(dc, "StatusBar", 0, NULL);
    if (!s_ctx.account) {
        LOG_E(TAG, "cannot create StatusBar account");
        return;
    }

    account_subscribe(s_ctx.account, "GPS");
    account_subscribe(s_ctx.account, "Clock");
    account_subscribe(s_ctx.account, "Power");
    account_set_callback(s_ctx.account, on_data_event);

    lv_obj_t *layer = lv_layer_top();

    s_ctx.cont = lv_obj_create(layer);
    lv_obj_remove_style_all(s_ctx.cont);
    lv_obj_set_size(s_ctx.cont, LV_HOR_RES, BAR_HEIGHT);
    lv_obj_set_pos(s_ctx.cont, 0, -BAR_HEIGHT);

    /* Style DEFAULT: transparent background (for map pages) */
    lv_obj_set_style_bg_opa(s_ctx.cont, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_ctx.cont, lv_color_hex(0x333333), LV_STATE_DEFAULT);

    /* Style USER_1: semi-opaque black (for normal pages) */
    lv_obj_set_style_bg_opa(s_ctx.cont, LV_OPA_60, LV_STATE_USER_1);
    lv_obj_set_style_bg_color(s_ctx.cont, lv_color_black(), LV_STATE_USER_1);
    lv_obj_set_style_shadow_color(s_ctx.cont, lv_color_black(), LV_STATE_USER_1);
    lv_obj_set_style_shadow_width(s_ctx.cont, 10, LV_STATE_USER_1);

    /* Transition between styles */
    static lv_style_transition_dsc_t tran;
    static const lv_style_prop_t props[] = {LV_STYLE_BG_COLOR, LV_STYLE_OPA, LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(&tran, props, lv_anim_path_ease_out, 200, 0, NULL);
    lv_obj_set_style_transition(s_ctx.cont, &tran, LV_STATE_USER_1);

    /* Common label style */
    static lv_style_t style_label;
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, lv_color_white());
    lv_style_set_text_font(&style_label, &lv_font_montserrat_16);

    s_ctx.img_sat = lv_img_create(s_ctx.cont);
    lv_img_set_src(s_ctx.img_sat, resource_pool_get_image("satellite"));
    lv_obj_align(s_ctx.img_sat, LV_ALIGN_LEFT_MID, 14, 0);

    s_ctx.label_sat = lv_label_create(s_ctx.cont);
    lv_obj_add_style(s_ctx.label_sat, &style_label, 0);
    lv_obj_align_to(s_ctx.label_sat, s_ctx.img_sat, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_label_set_text(s_ctx.label_sat, "0");

    s_ctx.img_sd = lv_img_create(s_ctx.cont);
    lv_img_set_src(s_ctx.img_sd, resource_pool_get_image("sd_card"));
    lv_obj_align(s_ctx.img_sd, LV_ALIGN_LEFT_MID, 55, -1);
    lv_obj_set_style_translate_y(s_ctx.img_sd, -BAR_HEIGHT, LV_STATE_DISABLED);

    static lv_style_transition_dsc_t tran_sd;
    static const lv_style_prop_t props_sd[] = {LV_STYLE_TRANSLATE_Y, LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(&tran_sd, props_sd, lv_anim_path_overshoot, 100, 0, NULL);
    lv_obj_set_style_transition(s_ctx.img_sd, &tran_sd, LV_STATE_DISABLED);
    lv_obj_set_style_transition(s_ctx.img_sd, &tran_sd, LV_STATE_DEFAULT);

    s_ctx.label_clock = lv_label_create(s_ctx.cont);
    lv_obj_add_style(s_ctx.label_clock, &style_label, 0);
    lv_label_set_text(s_ctx.label_clock, "00:00");
    lv_obj_center(s_ctx.label_clock);

    s_ctx.img_batt = lv_img_create(s_ctx.cont);
    lv_img_set_src(s_ctx.img_batt, resource_pool_get_image("battery"));
    lv_obj_align(s_ctx.img_batt, LV_ALIGN_RIGHT_MID, -35, 0);
    const lv_img_dsc_t *batt_dsc = resource_pool_get_image("battery");
    if (batt_dsc)
        lv_obj_set_size(s_ctx.img_batt, batt_dsc->header.w, batt_dsc->header.h);

    /* White fill bar inside battery icon */
    s_ctx.obj_usage = lv_obj_create(s_ctx.img_batt);
    lv_obj_remove_style_all(s_ctx.obj_usage);
    lv_obj_set_style_bg_color(s_ctx.obj_usage, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_ctx.obj_usage, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_ctx.obj_usage, LV_OPA_COVER, 0);
    lv_obj_set_size(s_ctx.obj_usage,
                    batt_dsc ? batt_dsc->header.w - BATT_USAGE_W_OFFSET : 10,
                    batt_dsc ? batt_dsc->header.h - BATT_USAGE_H_OFFSET : 10);
    lv_obj_align(s_ctx.obj_usage, LV_ALIGN_BOTTOM_MID, 0, -2);

    s_ctx.label_batt = lv_label_create(s_ctx.cont);
    lv_obj_add_style(s_ctx.label_batt, &style_label, 0);
    lv_obj_align_to(s_ctx.label_batt, s_ctx.img_batt, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_label_set_text(s_ctx.label_batt, "100");

    /* Initial state: transparent style */
    lv_obj_clear_state(s_ctx.cont, LV_STATE_USER_1);

    lv_timer_t *timer = lv_timer_create(on_timer, 1000, NULL);
    lv_timer_ready(timer);

    LOG_I(TAG, "StatusBar initialised");

    /* Start hidden — shown on STATUS_BAR_CMD_APPEAR from Startup */
    lv_obj_add_flag(s_ctx.cont, LV_OBJ_FLAG_HIDDEN);
}

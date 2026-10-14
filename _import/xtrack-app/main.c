/**
 * @file main.c
 * LVGL SDL2 simulator — entry point
 *
 * PROTOTYPE (round bezel + scripted screenshots): AuraS3's panel is a 466x466
 * ROUND AMOLED.  The simulator now runs at the target panel size, draws a
 * circular bezel mask (black outside the visible circle), and can export a
 * frame to PPM for layout review:
 *
 *   PROTO_SHOT=/tmp/shot.ppm PROTO_SHOT_DELAY=6000 ./build/lvgl-sim
 *
 * Both the bezel mask and the screenshot channel are throwaway scaffolding for
 * sorting out the round-screen layout; delete round_bezel_create() and
 * proto_capture() once the layout is settled.
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <unistd.h>

#include <lvgl.h>
#include "hal/hal.h"
#include "hal/hal_sdl.h"
#include "app.h"
#include "log.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "main"

/* Target panel: AuraS3 CO5300 AMOLED, 466x466, round (radius 233) */
#define PANEL_W 466
#define PANEL_H 466
#define PANEL_RADIUS (PANEL_W / 2)

static uint8_t s_bezel_buf[PANEL_W * PANEL_H * 4];
static lv_image_dsc_t s_bezel_dsc;

/*
 * Prototype: circular bezel
 * Opaque black outside the visible circle, fully transparent inside.
 * Must NOT be clickable: LVGL picks the topmost hit object as the press target,
 * so a clickable overlay would swallow every touch gesture.
 */
static void round_bezel_create(void)
{
    const double cx = PANEL_W / 2.0;
    const double cy = PANEL_H / 2.0;
    const double r2 = (double)PANEL_RADIUS * (double)PANEL_RADIUS;

    for (int y = 0; y < PANEL_H; y++) {
        for (int x = 0; x < PANEL_W; x++) {
            double dx = (x + 0.5) - cx;
            double dy = (y + 0.5) - cy;
            uint8_t *p = &s_bezel_buf[(y * PANEL_W + x) * 4];
            p[0] = p[1] = p[2] = 0x00;                     /* BGR: black */
            p[3] = (dx * dx + dy * dy > r2) ? 0xFF : 0x00;  /* alpha: outside */
        }
    }

    s_bezel_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    s_bezel_dsc.header.cf     = LV_COLOR_FORMAT_ARGB8888;
    s_bezel_dsc.header.flags  = 0;
    s_bezel_dsc.header.w      = PANEL_W;
    s_bezel_dsc.header.h      = PANEL_H;
    s_bezel_dsc.header.stride = PANEL_W * 4;
    s_bezel_dsc.data_size     = sizeof(s_bezel_buf);
    s_bezel_dsc.data          = s_bezel_buf;
    s_bezel_dsc.reserved      = NULL;

    lv_obj_t *img = lv_image_create(lv_layer_top());
    lv_image_set_src(img, &s_bezel_dsc);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(img, 0, 0);
}

/*
 * Prototype: scripted screenshot
 * Composes the active screen with the top layer (status bar lives there), applies
 * the circular bezel, and writes a binary PPM (P6).  This is what the physical
 * panel shows, so "is it cut off?" can be checked without the board.
 */
/* Walk the object tree and report every visible element whose bounding box
 * sticks out of the round visible area -- the mechanical form of the
 * "information must not be clipped" acceptance criterion. */
static lv_obj_tree_walk_res_t proto_clip_cb(lv_obj_t *obj, void *user_data)
{
    int *counters = (int *)user_data;
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN))
        return LV_OBJ_TREE_WALK_NEXT;
    /* Full-screen layer/background objects are meant to bleed off the round area */
    if (obj == lv_screen_active() || obj == lv_layer_top() || obj == lv_layer_sys() ||
        obj == lv_layer_bottom())
        return LV_OBJ_TREE_WALK_NEXT;

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    if (a.x2 <= a.x1 || a.y2 <= a.y1)
        return LV_OBJ_TREE_WALK_NEXT;

    const double cx = PANEL_W / 2.0, cy = PANEL_H / 2.0;
    const double r2 = (double)PANEL_RADIUS * (double)PANEL_RADIUS;

    const int xs[2] = {a.x1, a.x2};
    const int ys[2] = {a.y1, a.y2};
    double worst = 0;
    int wx = 0, wy = 0;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            double dx = (xs[i] + 0.5) - cx, dy = (ys[j] + 0.5) - cy;
            double d  = sqrt(dx * dx + dy * dy) - PANEL_RADIUS;
            if (d > worst) { worst = d; wx = xs[i]; wy = ys[j]; }
        }
    }
    /* chord width available at the object's vertical band */
    int y_top = a.y1 < 0 ? 0 : a.y1;
    int y_bot = a.y2 > PANEL_H - 1 ? PANEL_H - 1 : a.y2;
    double dy_worst = 0;
    for (int y = y_top; y <= y_bot; y++) {
        double dy = fabs((y + 0.5) - cy);
        double chord = 2.0 * sqrt(fmax(r2 - dy * dy, 0.0));
        double over = (a.x2 - a.x1 + 1) - chord;
        if (over > dy_worst) dy_worst = over;
    }

    if (worst > 1.0) {
        (*counters)++;
        LOG_W(TAG, "CLIP  x=%4d..%4d y=%4d..%4d  corner(%d,%d) outside by %4.0fpx | needs width %d, chord allows %.0f (over %.0f)",
              a.x1, a.x2, a.y1, a.y2, wx, wy, worst,
              a.x2 - a.x1 + 1, (a.x2 - a.x1 + 1) - dy_worst, dy_worst);
    } else {
        counters[1]++;
    }
    return LV_OBJ_TREE_WALK_NEXT;
}

static bool proto_write_ppm(const char *path)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *top = lv_layer_top();

    lv_draw_buf_t *base = lv_snapshot_take(scr, LV_COLOR_FORMAT_ARGB8888);
    lv_draw_buf_t *over = lv_snapshot_take(top, LV_COLOR_FORMAT_ARGB8888);
    if (!base) {
        LOG_E(TAG, "snapshot of the screen failed");
        return false;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        LOG_E(TAG, "cannot open %s", path);
        lv_draw_buf_destroy(base);
        if (over) lv_draw_buf_destroy(over);
        return false;
    }
    fprintf(f, "P6\n%d %d\n255\n", PANEL_W, PANEL_H);

    const int   bw = base->header.w, bh = base->header.h;
    const int   bs = base->header.stride;
    const uint8_t *bd = base->data;
    const int   ow = over ? over->header.w : 0;
    const int   os = over ? over->header.stride : 0;
    const uint8_t *od = over ? over->data : NULL;
    const double cx = PANEL_W / 2.0, cy = PANEL_H / 2.0;
    const double r2 = (double)PANEL_RADIUS * (double)PANEL_RADIUS;

    for (int y = 0; y < PANEL_H; y++) {
        for (int x = 0; x < PANEL_W; x++) {
            /* LVGL ARGB8888 in memory is B, G, R, A */
            const uint8_t *bp = (x < bw && y < bh) ? bd + y * bs + x * 4 : NULL;
            int r = bp ? bp[2] : 0, g = bp ? bp[1] : 0, b = bp ? bp[0] : 0;

            if (od && x < ow && y < over->header.h) {
                const uint8_t *op = od + y * os + x * 4;
                int a = op[3];
                if (a) {   /* straight-alpha "over" */
                    r = (op[2] * a + r * (255 - a)) / 255;
                    g = (op[1] * a + g * (255 - a)) / 255;
                    b = (op[0] * a + b * (255 - a)) / 255;
                }
            }

            double dx = (x + 0.5) - cx, dy = (y + 0.5) - cy;
            if (dx * dx + dy * dy > r2) {   /* physical bezel: black */
                r = g = b = 0;
            }

            uint8_t px[3] = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
            fwrite(px, 1, 3, f);
        }
    }

    fclose(f);
    lv_draw_buf_destroy(base);
    if (over) lv_draw_buf_destroy(over);
    LOG_I(TAG, "screenshot written: %s", path);
    return true;
}

int main(void)
{
    lv_init();
    hal_init();

    lv_display_t *disp = hal_init_display(PANEL_W, PANEL_H);
    (void)disp;

    app_init();
    round_bezel_create();

    const char *shot_path  = getenv("PROTO_SHOT");
    uint32_t    shot_delay = 6000;
    if (getenv("PROTO_SHOT_DELAY"))
        shot_delay = (uint32_t)atoi(getenv("PROTO_SHOT_DELAY"));

    LOG_I(TAG, "LVGL simulator started (%dx%d, round bezel mask)", PANEL_W, PANEL_H);

    while (1) {
        uint32_t delay = lv_timer_handler();
        if (delay == LV_NO_TIMER_READY)
            delay = LV_DEF_REFR_PERIOD;

        if (shot_path && lv_tick_get() >= shot_delay) {
            int counters[2] = {0, 0};
            LOG_I(TAG, "---- round-screen clip report (visible area: circle r=%d) ----", PANEL_RADIUS);
            lv_obj_tree_walk(lv_screen_active(), proto_clip_cb, counters);
            lv_obj_tree_walk(lv_layer_top(), proto_clip_cb, counters);
            LOG_I(TAG, "---- clipped: %d, fully inside: %d ----", counters[0], counters[1]);
            proto_write_ppm(shot_path);
            return 0;
        }
        usleep(delay * 1000);
    }

    return 0;
}

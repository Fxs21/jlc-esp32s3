#include "hal_sdl.h"

lv_display_t *hal_init_display(int32_t w, int32_t h)
{
    /* Create a display backed by an SDL window */
    lv_display_t *disp = lv_sdl_window_create(w, h);

    /* Mouse pointer input — NOT assigned to group (clicks work independently) */
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, disp);

    /* Mouse wheel — assigned to group as ENCODER type.
     * Scrolling the wheel navigates through group items (X-Track pattern). */
    lv_indev_t *wheel = lv_sdl_mousewheel_create();
    lv_indev_set_display(wheel, disp);

    /* Keyboard input — assigned to group for keyboard navigation */
    lv_indev_t *kb = lv_sdl_keyboard_create();
    lv_indev_set_display(kb, disp);

    /* Create default group and assign navigational indevs to it.
     * This enables SystemInfos item focus animations via wheel/keyboard. */
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(wheel, g);
    lv_indev_set_group(kb, g);

    /* Set this as the default display */
    lv_display_set_default(disp);

    return disp;
}

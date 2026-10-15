/**
 * @file  hal_sdl.h
 * @brief PC simulator only -- SDL2 display and input devices
 *
 * This header is NOT part of the platform-neutral HAL interface.
 * The board port provides its own display/input init through the BSP,
 * so nothing in the App layer (page_manager, pages, data_center,
 * data_proc, resource, utils) may include this file.
 */
#ifndef HAL_SDL_H
#define HAL_SDL_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialize SDL2 display, mouse, and keyboard for LVGL
 * @param  w: Window width in pixels
 * @param  h: Window height in pixels
 * @retval Pointer to the created LVGL display object
 * @note   Internally calls lv_sdl_window_create(), lv_sdl_mouse_create(),
 *         lv_sdl_mousewheel_create(), and lv_sdl_keyboard_create().
 *         The created display is set as the default.
 */
lv_display_t *hal_init_display(int32_t w, int32_t h);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SDL_H */

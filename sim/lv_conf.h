/**
 * @file lv_conf.h
 * LVGL v9.6 configuration — minimal set for simulator + learning
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* clang-format off */
#if 1

/** Use C library sprintf so that %f formatting works in lv_label_set_text_fmt */
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/** PC simulator: use libc malloc instead of LVGL's built-in pool, so full-screen
 *  allocations (snapshot of a 466x466 ARGB8888 frame ≈ 868 KB) can succeed.
 *  Board builds use Kconfig and are unaffected. */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB

#define LV_DEF_REFR_PERIOD 16  /* 1000/16 = 62.5 fps (default 33) */

#define LV_USE_LOG 1
#if LV_USE_LOG
    #define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF   1
#endif

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MISC 1

#define LV_USE_FS_STDIO 1
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER   'A'
    #define LV_FS_STDIO_PATH     ""
    #define LV_FS_STDIO_CACHE_SIZE 0
#endif

#define LV_USE_LABEL       1
#define LV_USE_BUTTON      1
#define LV_USE_LINE        1
#define LV_USE_ANIMATION   1

#define LV_USE_FLEX        1

#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_28  1
#define LV_FONT_MONTSERRAT_32  1
#define LV_FONT_MONTSERRAT_48  1
#define LV_FONT_DEFAULT        &lv_font_montserrat_14

/*
 * MONITOR (FPS / memory)
 * Requires LV_USE_OBSERVER
 */
#define LV_USE_OBSERVER    1
#define LV_USE_SYSMON      1
#define LV_USE_PERF_MONITOR 1
#if LV_USE_PERF_MONITOR
    #define LV_USE_PERF_MONITOR_POS LV_ALIGN_RIGHT_MID
    #define LV_USE_PERF_MONITOR_LOG_MODE 0
#endif
#define LV_USE_MEM_MONITOR 1
#if LV_USE_MEM_MONITOR
    #define LV_USE_MEM_MONITOR_POS LV_ALIGN_LEFT_MID
#endif

#define LV_USE_SDL 1
#if LV_USE_SDL
    #define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
    #define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_DIRECT
    #define LV_SDL_BUF_COUNT        1
    #define LV_SDL_ACCELERATED      1
    #define LV_SDL_FULLSCREEN       0
    #define LV_SDL_DIRECT_EXIT      1
    #define LV_SDL_MOUSEWHEEL_MODE  LV_SDL_MOUSEWHEEL_MODE_ENCODER
#endif

/** Disable default theme to remove borders/padding from all objects.
 *  Pages control their own styling via root_default_style and on_load. */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE  0
#define LV_USE_THEME_MONO    0

/** Snapshot API — used by the simulator's scripted screenshot channel
 *  (PROTO_SHOT env var in main.c) to export frames for layout review. */
#define LV_USE_SNAPSHOT 1

#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_LZ4_INTERNAL 0

#endif /*content enable*/
#endif /*LV_CONF_H*/

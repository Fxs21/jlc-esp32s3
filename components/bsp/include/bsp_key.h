#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_key_s *bsp_key_handle_t;

typedef struct {
    bool present;
} bsp_key_desc_t;

const bsp_key_desc_t *bsp_key_get_desc(void);

// The key is the board's GPIO-wired user key (the BOOT key on both current
// boards); active level and pull-up are normalized by the board port.
// On failure, *handle_out is set to NULL after handle_out is validated.
esp_err_t bsp_key_open(bsp_key_handle_t *handle_out);
esp_err_t bsp_key_close(bsp_key_handle_t handle);
// true = pressed. Instantaneous level sample: no debounce and no events,
// callers own their own timing.
esp_err_t bsp_key_is_pressed(bsp_key_handle_t handle, bool *pressed_out);

#ifdef __cplusplus
}
#endif

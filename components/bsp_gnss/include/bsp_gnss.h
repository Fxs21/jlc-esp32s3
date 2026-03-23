#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool present;
} bsp_gnss_desc_t;

typedef struct bsp_gnss_s *bsp_gnss_handle_t;

const bsp_gnss_desc_t *bsp_gnss_get_desc(void);

esp_err_t bsp_gnss_open(bsp_gnss_handle_t *out_handle);
esp_err_t bsp_gnss_close(bsp_gnss_handle_t handle);
esp_err_t bsp_gnss_read(bsp_gnss_handle_t handle, uint8_t *buf, size_t len, size_t *out_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

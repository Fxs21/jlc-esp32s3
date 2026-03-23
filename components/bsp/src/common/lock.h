#pragma once

#include <stdint.h>

#include "esp_check.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Shared lock helpers.  Each compilation unit declares its own static
// SemaphoreHandle_t / StaticSemaphore_t / portMUX_TYPE and provides a
// thin lock() / unlock() wrapper.  The LOCK_TIMEOUT_MS macro must be
// defined before including this header.

#ifdef __cplusplus
extern "C" {
#endif

static inline esp_err_t bsp_lock_ensure(SemaphoreHandle_t *lock, StaticSemaphore_t *buf,
                                         portMUX_TYPE *mux, const char *tag)
{
    if (*lock != NULL) {
        return ESP_OK;
    }
    taskENTER_CRITICAL(mux);
    if (*lock == NULL) {
        *lock = xSemaphoreCreateMutexStatic(buf);
    }
    taskEXIT_CRITICAL(mux);
    ESP_RETURN_ON_FALSE(*lock != NULL, ESP_ERR_NO_MEM, tag, "create lock failed");
    return ESP_OK;
}

static inline esp_err_t bsp_lock_take(SemaphoreHandle_t *lock, StaticSemaphore_t *buf,
                                       portMUX_TYPE *mux, const char *tag,
                                       uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(bsp_lock_ensure(lock, buf, mux, tag), tag, "lock not ready");
    TickType_t ticks = timeout_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(*lock, ticks) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static inline void bsp_lock_give(SemaphoreHandle_t lock)
{
    if (lock != NULL) {
        (void)xSemaphoreGive(lock);
    }
}

#ifdef __cplusplus
}
#endif

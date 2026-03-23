#pragma once

#include <stdbool.h>

#include "esp_camera.h"
#include "esp_err.h"
#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_camera_s *bsp_camera_handle_t;

bool bsp_camera_present(void);

esp_err_t bsp_camera_new(bsp_camera_handle_t *out_handle);
esp_err_t bsp_camera_start(bsp_camera_handle_t handle);
esp_err_t bsp_camera_fb_get(bsp_camera_handle_t handle, camera_fb_t **out_frame);
esp_err_t bsp_camera_fb_return(bsp_camera_handle_t handle, camera_fb_t *frame);
esp_err_t bsp_camera_stop(bsp_camera_handle_t handle);
sensor_t *bsp_camera_sensor(bsp_camera_handle_t handle);
esp_err_t bsp_camera_del(bsp_camera_handle_t handle);

#ifdef __cplusplus
}
#endif

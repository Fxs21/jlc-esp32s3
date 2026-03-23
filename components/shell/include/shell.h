#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *prompt;
    const char *initial_path;
} shell_cfg_t;

shell_cfg_t shell_default_config(void);

esp_err_t shell_init(const shell_cfg_t *cfg);
esp_err_t shell_deinit(void);

esp_err_t shell_mount_add(const char *mount_path);
esp_err_t shell_mount_remove(const char *mount_path);

#ifdef __cplusplus
}
#endif

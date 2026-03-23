#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHELL_PATH_MAX 256

const char *shell_get_cwd(void);
esp_err_t shell_set_cwd(const char *path);

esp_err_t shell_fs_register_commands(void);
esp_err_t shell_path_resolve(const char *cwd, const char *path, char *out_path, size_t out_size);

size_t shell_mount_get_count_internal(void);
const char *shell_mount_get_path_internal(size_t index);

#ifdef __cplusplus
}
#endif

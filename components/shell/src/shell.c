#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"

#include "shell_internal.h"
#include "shell.h"

#define TAG "shell"
#define SHELL_ROOT_PATH "/"
#define SHELL_MAX_MOUNTS 8

typedef enum {
    SHELL_STATE_STOPPED = 0,
    SHELL_STATE_RUNNING,
} shell_state_t;

typedef struct {
    esp_console_repl_t *repl;
    shell_state_t state;
    char cwd_path[SHELL_PATH_MAX];
    char mount_paths[SHELL_MAX_MOUNTS][SHELL_PATH_MAX];
    size_t mount_count;
} shell_runtime_t;

static shell_runtime_t shell_rt = {
    .repl = NULL,
    .state = SHELL_STATE_STOPPED,
    .cwd_path = "/",
    .mount_paths = {{0}},
    .mount_count = 0,
};

static void shell_copy_path(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        src = SHELL_ROOT_PATH;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static esp_err_t destroy_repl(void)
{
    if (shell_rt.repl == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    if (shell_rt.repl->del != NULL) {
        ret = shell_rt.repl->del(shell_rt.repl);
    }
    shell_rt.repl = NULL;
    return ret;
}

static bool shell_path_is_root(const char *path)
{
    return (path != NULL && strcmp(path, SHELL_ROOT_PATH) == 0);
}

static int shell_find_mount_index(const char *mount_path)
{
    for (size_t i = 0; i < shell_rt.mount_count; i++) {
        if (strcmp(shell_rt.mount_paths[i], mount_path) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static esp_err_t shell_normalize_mount_path(const char *mount_path, char *out_path, size_t out_size)
{
    ESP_RETURN_ON_FALSE(mount_path != NULL, ESP_ERR_INVALID_ARG, TAG, "mount path is null");
    ESP_RETURN_ON_FALSE(out_path != NULL, ESP_ERR_INVALID_ARG, TAG, "out path is null");
    ESP_RETURN_ON_FALSE(mount_path[0] == '/', ESP_ERR_INVALID_ARG, TAG, "mount path must be absolute");
    return shell_path_resolve(SHELL_ROOT_PATH, mount_path, out_path, out_size);
}

shell_cfg_t shell_default_config(void)
{
    const shell_cfg_t cfg = {
        .prompt = "$",
        .initial_path = SHELL_ROOT_PATH,
    };
    return cfg;
}

static esp_err_t create_repl(const char *prompt)
{
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = prompt;

#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
    esp_console_dev_uart_config_t dev_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    return esp_console_new_repl_uart(&dev_cfg, &repl_cfg, &shell_rt.repl);
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t dev_cfg = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_serial_jtag(&dev_cfg, &repl_cfg, &shell_rt.repl);
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t dev_cfg = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_cdc(&dev_cfg, &repl_cfg, &shell_rt.repl);
#else
    (void)prompt;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t shell_init(const shell_cfg_t *cfg)
{
    ESP_RETURN_ON_FALSE(shell_rt.state == SHELL_STATE_STOPPED, ESP_ERR_INVALID_STATE, TAG, "shell already initialized");

    shell_cfg_t local_cfg = shell_default_config();
    if (cfg != NULL) {
        if (cfg->prompt != NULL) {
            local_cfg.prompt = cfg->prompt;
        }
        if (cfg->initial_path != NULL && cfg->initial_path[0] != '\0') {
            local_cfg.initial_path = cfg->initial_path;
        }
    }

    char initial_path[SHELL_PATH_MAX] = {0};
    esp_err_t ret = shell_path_resolve(SHELL_ROOT_PATH, local_cfg.initial_path, initial_path, sizeof(initial_path));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "invalid initial path '%s', fallback to /", local_cfg.initial_path);
        shell_copy_path(initial_path, sizeof(initial_path), SHELL_ROOT_PATH);
    }

    if (!shell_path_is_root(initial_path)) {
        struct stat st = {0};
        if (stat(initial_path, &st) != 0) {
            ESP_LOGW(TAG, "initial path '%s' unavailable (errno=%d), fallback to /", initial_path, errno);
            shell_copy_path(initial_path, sizeof(initial_path), SHELL_ROOT_PATH);
        } else if (!S_ISDIR(st.st_mode)) {
            ESP_LOGW(TAG, "initial path '%s' is not directory, fallback to /", initial_path);
            shell_copy_path(initial_path, sizeof(initial_path), SHELL_ROOT_PATH);
        }
    }

    ret = create_repl(local_cfg.prompt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create repl failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = shell_fs_register_commands();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register fs commands failed: %s", esp_err_to_name(ret));
        (void)destroy_repl();
        return ret;
    }

    ret = esp_console_start_repl(shell_rt.repl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start repl failed: %s", esp_err_to_name(ret));
        (void)destroy_repl();
        return ret;
    }

    shell_copy_path(shell_rt.cwd_path, sizeof(shell_rt.cwd_path), initial_path);
    shell_rt.state = SHELL_STATE_RUNNING;
    return ESP_OK;
}

esp_err_t shell_deinit(void)
{
    ESP_RETURN_ON_FALSE(shell_rt.state != SHELL_STATE_STOPPED, ESP_ERR_INVALID_STATE, TAG, "shell is not initialized");
    esp_err_t first_err = ESP_OK;

    if (shell_rt.state == SHELL_STATE_RUNNING && shell_rt.repl != NULL) {
        esp_err_t ret = esp_console_stop_repl(shell_rt.repl);
        if (ret != ESP_OK && first_err == ESP_OK) {
            first_err = ret;
        }
    }

    esp_err_t ret = destroy_repl();
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    shell_rt = (shell_runtime_t){
        .repl = NULL,
        .state = SHELL_STATE_STOPPED,
        .cwd_path = "/",
        .mount_paths = {{0}},
        .mount_count = 0,
    };
    return first_err;
}

const char *shell_get_cwd(void)
{
    return shell_rt.cwd_path;
}

esp_err_t shell_set_cwd(const char *path)
{
    ESP_RETURN_ON_FALSE(shell_rt.state != SHELL_STATE_STOPPED, ESP_ERR_INVALID_STATE, TAG, "shell is not initialized");
    ESP_RETURN_ON_FALSE(path != NULL, ESP_ERR_INVALID_ARG, TAG, "path is null");

    char resolved[SHELL_PATH_MAX] = {0};
    ESP_RETURN_ON_ERROR(shell_path_resolve(shell_rt.cwd_path, path, resolved, sizeof(resolved)),
                        TAG, "resolve cwd failed");

    if (shell_path_is_root(resolved)) {
        shell_copy_path(shell_rt.cwd_path, sizeof(shell_rt.cwd_path), resolved);
        return ESP_OK;
    }

    struct stat st = {0};
    if (stat(resolved, &st) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!S_ISDIR(st.st_mode)) {
        return ESP_ERR_INVALID_ARG;
    }

    shell_copy_path(shell_rt.cwd_path, sizeof(shell_rt.cwd_path), resolved);
    return ESP_OK;
}

esp_err_t shell_mount_add(const char *mount_path)
{
    ESP_RETURN_ON_FALSE(shell_rt.state == SHELL_STATE_STOPPED, ESP_ERR_INVALID_STATE, TAG, "mount table is read-only after init");

    char normalized[SHELL_PATH_MAX] = {0};
    ESP_RETURN_ON_ERROR(shell_normalize_mount_path(mount_path, normalized, sizeof(normalized)),
                        TAG, "normalize mount path failed");
    ESP_RETURN_ON_FALSE(!shell_path_is_root(normalized), ESP_ERR_INVALID_ARG, TAG, "root path is reserved");

    if (shell_find_mount_index(normalized) >= 0) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(shell_rt.mount_count < SHELL_MAX_MOUNTS, ESP_ERR_NO_MEM, TAG, "mount table is full");

    strncpy(shell_rt.mount_paths[shell_rt.mount_count], normalized, sizeof(shell_rt.mount_paths[shell_rt.mount_count]) - 1);
    shell_rt.mount_paths[shell_rt.mount_count][sizeof(shell_rt.mount_paths[shell_rt.mount_count]) - 1] = '\0';
    shell_rt.mount_count++;
    return ESP_OK;
}

esp_err_t shell_mount_remove(const char *mount_path)
{
    ESP_RETURN_ON_FALSE(shell_rt.state == SHELL_STATE_STOPPED, ESP_ERR_INVALID_STATE, TAG, "mount table is read-only after init");

    char normalized[SHELL_PATH_MAX] = {0};
    ESP_RETURN_ON_ERROR(shell_normalize_mount_path(mount_path, normalized, sizeof(normalized)),
                        TAG, "normalize mount path failed");

    int index = shell_find_mount_index(normalized);
    if (index < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t i = (size_t)index;
    while ((i + 1) < shell_rt.mount_count) {
        memcpy(shell_rt.mount_paths[i], shell_rt.mount_paths[i + 1], sizeof(shell_rt.mount_paths[i]));
        i++;
    }
    memset(shell_rt.mount_paths[shell_rt.mount_count - 1], 0, sizeof(shell_rt.mount_paths[shell_rt.mount_count - 1]));
    shell_rt.mount_count--;
    return ESP_OK;
}

size_t shell_mount_get_count_internal(void)
{
    return shell_rt.mount_count;
}

const char *shell_mount_get_path_internal(size_t index)
{
    if (index >= shell_rt.mount_count) {
        return NULL;
    }
    return shell_rt.mount_paths[index];
}

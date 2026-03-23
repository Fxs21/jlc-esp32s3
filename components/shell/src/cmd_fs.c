#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_console.h"

#include "shell_internal.h"
#include "shell.h"

#define TAG "shell_fs"
#define LS_SIZE_WIDTH 10
#define SHELL_ROOT_PATH "/"

static int cmd_ls_root(void)
{
    size_t mount_count = shell_mount_get_count_internal();
    if (mount_count == 0) {
        printf("(no mounts)\n");
        return 0;
    }

    for (size_t i = 0; i < mount_count; i++) {
        const char *mount_path = shell_mount_get_path_internal(i);
        if (mount_path == NULL || mount_path[0] == '\0') {
            continue;
        }
        const char *display_name = mount_path;
        if (display_name[0] == '/') {
            display_name++;
        }
        if (display_name[0] == '\0') {
            continue;
        }
        printf("d %*" PRIu64 " %s/\n", LS_SIZE_WIDTH, (uint64_t)0, display_name);
    }
    return 0;
}

static int cmd_pwd(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        printf("usage: pwd\n");
        return 1;
    }

    printf("%s\n", shell_get_cwd());
    return 0;
}

static int cmd_cd(int argc, char **argv)
{
    if (argc > 2) {
        printf("usage: cd [path]\n");
        return 1;
    }

    const char *input_path = (argc == 2) ? argv[1] : SHELL_ROOT_PATH;
    esp_err_t ret = shell_set_cwd(input_path);
    if (ret == ESP_ERR_NOT_FOUND) {
        printf("cd: %s: %s\n", input_path, strerror(ENOENT));
        return 1;
    }
    if (ret == ESP_ERR_INVALID_ARG) {
        printf("cd: %s: not a directory or invalid path\n", input_path);
        return 1;
    }
    if (ret != ESP_OK) {
        printf("cd: failed to set cwd: %s\n", esp_err_to_name(ret));
        return 1;
    }

    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    if (argc > 2) {
        printf("usage: ls [path]\n");
        return 1;
    }

    const char *input_path = (argc == 2) ? argv[1] : ".";
    char resolved[SHELL_PATH_MAX] = {0};
    esp_err_t ret = shell_path_resolve(shell_get_cwd(), input_path, resolved, sizeof(resolved));
    if (ret != ESP_OK) {
        printf("ls: invalid path: %s\n", input_path);
        return 1;
    }
    if (strcmp(resolved, SHELL_ROOT_PATH) == 0) {
        return cmd_ls_root();
    }

    DIR *dir = opendir(resolved);
    if (dir == NULL) {
        printf("ls: %s: %s\n", resolved, strerror(errno));
        return 1;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        char entry_path[SHELL_PATH_MAX] = {0};
        int len = snprintf(entry_path, sizeof(entry_path), "%s/%s", resolved, entry->d_name);
        if (len < 0 || len >= (int)sizeof(entry_path)) {
            printf("? %*s %s\n", LS_SIZE_WIDTH, "?", entry->d_name);
            continue;
        }

        struct stat st = {0};
        if (stat(entry_path, &st) != 0) {
            printf("? %*s %s\n", LS_SIZE_WIDTH, "?", entry->d_name);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            printf("d %*" PRIu64 " %s/\n", LS_SIZE_WIDTH, (uint64_t)st.st_size, entry->d_name);
        } else {
            printf("- %*" PRIu64 " %s\n", LS_SIZE_WIDTH, (uint64_t)st.st_size, entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}

esp_err_t shell_fs_register_commands(void)
{
    const esp_console_cmd_t pwd_cmd = {
        .command = "pwd",
        .help = "print current directory",
        .hint = NULL,
        .func = &cmd_pwd,
    };
    const esp_console_cmd_t cd_cmd = {
        .command = "cd",
        .help = "change directory",
        .hint = NULL,
        .func = &cmd_cd,
    };
    const esp_console_cmd_t ls_cmd = {
        .command = "ls",
        .help = "list directory entries",
        .hint = NULL,
        .func = &cmd_ls,
    };

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&pwd_cmd), TAG, "register pwd failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&cd_cmd), TAG, "register cd failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&ls_cmd), TAG, "register ls failed");
    return ESP_OK;
}

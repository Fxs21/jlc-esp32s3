#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_console.h"

#include "shell_internal.h"
#include "shell.h"

#define TAG "shell_fs"
#define LS_SIZE_WIDTH 10
#define SHELL_ROOT_PATH "/"
#define CAT_BUF_SIZE 128
#define HEXDUMP_BUF_SIZE 16
#define HEXDUMP_DEFAULT_LEN 256
#define HEXDUMP_MAX_LEN 4096

static bool parse_size_arg(const char *text, size_t min_value, size_t max_value, size_t *out_value)
{
    if (text == NULL || out_value == NULL) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value < min_value || value > max_value) {
        return false;
    }

    *out_value = (size_t)value;
    return true;
}

static bool resolve_path_arg(const char *cmd, const char *input_path, char *resolved, size_t resolved_size)
{
    esp_err_t ret = shell_path_resolve(shell_get_cwd(), input_path, resolved, resolved_size);
    if (ret != ESP_OK) {
        printf("%s: invalid path: %s\n", cmd, input_path);
        return false;
    }
    if (strcmp(resolved, SHELL_ROOT_PATH) == 0) {
        printf("%s: /: virtual root is not a file system path\n", cmd);
        return false;
    }
    return true;
}

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

static int cmd_cat(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: cat <path>\n");
        return 1;
    }

    char resolved[SHELL_PATH_MAX] = {0};
    if (!resolve_path_arg("cat", argv[1], resolved, sizeof(resolved))) {
        return 1;
    }

    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL) {
        printf("cat: %s: %s\n", resolved, strerror(errno));
        return 1;
    }

    uint8_t buf[CAT_BUF_SIZE];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        for (size_t i = 0; i < n; i++) {
            putchar(buf[i]);
        }
    }
    if (ferror(fp)) {
        printf("\ncat: %s: read error\n", resolved);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

static int cmd_hexdump(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        printf("usage: hexdump <path> [len]\n");
        return 1;
    }

    size_t limit = HEXDUMP_DEFAULT_LEN;
    if (argc == 3 && !parse_size_arg(argv[2], 1, HEXDUMP_MAX_LEN, &limit)) {
        printf("hexdump: invalid len, range is 1..%u\n", (unsigned)HEXDUMP_MAX_LEN);
        return 1;
    }

    char resolved[SHELL_PATH_MAX] = {0};
    if (!resolve_path_arg("hexdump", argv[1], resolved, sizeof(resolved))) {
        return 1;
    }

    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL) {
        printf("hexdump: %s: %s\n", resolved, strerror(errno));
        return 1;
    }

    uint8_t buf[HEXDUMP_BUF_SIZE];
    size_t offset = 0;
    while (offset < limit) {
        size_t want = limit - offset;
        if (want > sizeof(buf)) {
            want = sizeof(buf);
        }
        size_t n = fread(buf, 1, want, fp);
        if (n == 0) {
            break;
        }

        printf("%08x  ", (unsigned)offset);
        for (size_t i = 0; i < sizeof(buf); i++) {
            if (i < n) {
                printf("%02x ", buf[i]);
            } else {
                printf("   ");
            }
            if (i == 7) {
                printf(" ");
            }
        }
        printf(" |");
        for (size_t i = 0; i < n; i++) {
            uint8_t c = buf[i];
            putchar((c >= 32 && c <= 126) ? c : '.');
        }
        printf("|\n");
        offset += n;
    }

    if (ferror(fp)) {
        printf("hexdump: %s: read error\n", resolved);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

static int cmd_mkdir(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: mkdir <path>\n");
        return 1;
    }

    char resolved[SHELL_PATH_MAX] = {0};
    if (!resolve_path_arg("mkdir", argv[1], resolved, sizeof(resolved))) {
        return 1;
    }

    if (mkdir(resolved, 0775) != 0) {
        printf("mkdir: %s: %s\n", resolved, strerror(errno));
        return 1;
    }
    return 0;
}

static int cmd_rm(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: rm <path>\n");
        return 1;
    }

    char resolved[SHELL_PATH_MAX] = {0};
    if (!resolve_path_arg("rm", argv[1], resolved, sizeof(resolved))) {
        return 1;
    }

    struct stat st = {0};
    if (stat(resolved, &st) != 0) {
        printf("rm: %s: %s\n", resolved, strerror(errno));
        return 1;
    }

    int ret = S_ISDIR(st.st_mode) ? rmdir(resolved) : unlink(resolved);
    if (ret != 0) {
        printf("rm: %s: %s\n", resolved, strerror(errno));
        return 1;
    }
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
    const esp_console_cmd_t cat_cmd = {
        .command = "cat",
        .help = "print file content",
        .hint = NULL,
        .func = &cmd_cat,
    };
    const esp_console_cmd_t hexdump_cmd = {
        .command = "hexdump",
        .help = "print file bytes as hex",
        .hint = NULL,
        .func = &cmd_hexdump,
    };
    const esp_console_cmd_t mkdir_cmd = {
        .command = "mkdir",
        .help = "create directory",
        .hint = NULL,
        .func = &cmd_mkdir,
    };
    const esp_console_cmd_t rm_cmd = {
        .command = "rm",
        .help = "remove file or empty directory",
        .hint = NULL,
        .func = &cmd_rm,
    };

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&pwd_cmd), TAG, "register pwd failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&cd_cmd), TAG, "register cd failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&ls_cmd), TAG, "register ls failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&cat_cmd), TAG, "register cat failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&hexdump_cmd), TAG, "register hexdump failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&mkdir_cmd), TAG, "register mkdir failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&rm_cmd), TAG, "register rm failed");
    return ESP_OK;
}

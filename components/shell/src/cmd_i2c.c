#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bsp_i2c.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_err.h"
#include "shell_internal.h"

#define TAG "shell_i2c"
#define I2C_SCAN_ADDR_CAPACITY (BSP_I2C_SCAN_LAST_ADDR - BSP_I2C_SCAN_FIRST_ADDR + 1)

static void print_i2c_scan_usage(void)
{
    printf("usage: i2c scan\n");
    printf("note: scans the BSP default I2C bus via public BSP API\n");
}

static void print_scan_header(void)
{
    printf("     00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");
}

static bool address_found(uint8_t address, const uint8_t *addresses, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (addresses[i] == address) {
            return true;
        }
    }
    return false;
}

static int run_i2c_scan(void)
{
    uint8_t addresses[I2C_SCAN_ADDR_CAPACITY] = {0};
    size_t found = 0;
    esp_err_t ret = bsp_i2c_scan(addresses, I2C_SCAN_ADDR_CAPACITY, &found, BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("i2c scan failed: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("i2c scan: bsp default bus timeout=%" PRIu32 "ms\n", (uint32_t)BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS);
    print_scan_header();
    for (uint8_t row = 0; row <= 0x70; row += 0x10) {
        printf("%02x:", row);
        for (uint8_t col = 0; col < 0x10; col++) {
            uint8_t addr = row + col;
            if (addr < BSP_I2C_SCAN_FIRST_ADDR || addr > BSP_I2C_SCAN_LAST_ADDR) {
                printf("   ");
            } else if (address_found(addr, addresses, found)) {
                printf(" %02x", addr);
            } else {
                printf(" --");
            }
        }
        printf("\n");
    }
    printf("found: %u device(s)\n", (unsigned)found);
    return 0;
}

static int cmd_i2c(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "scan") != 0) {
        print_i2c_scan_usage();
        return 1;
    }
    return run_i2c_scan();
}

static int cmd_i2c_scan(int argc, char **argv)
{
    if (argc != 1) {
        print_i2c_scan_usage();
        return 1;
    }
    return run_i2c_scan();
}

esp_err_t shell_i2c_register_commands(void)
{
    const esp_console_cmd_t i2c_cmd = {
        .command = "i2c",
        .help = "I2C diagnostic commands: i2c scan",
        .hint = NULL,
        .func = &cmd_i2c,
    };
    const esp_console_cmd_t i2c_scan_cmd = {
        .command = "i2c_scan",
        .help = "scan BSP default I2C bus",
        .hint = NULL,
        .func = &cmd_i2c_scan,
    };

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&i2c_cmd), TAG, "register i2c failed");
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&i2c_scan_cmd), TAG, "register i2c_scan failed");
    return ESP_OK;
}

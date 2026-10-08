// test/i2c: bsp_i2c public API 自检与总线诊断.
// 只使用 BSP public API; 期望设备表来自 docs/hw/boards/<board>/truth_table.md 的
// I2C 清单, 以 truth table 为准, 硬件变化后两处同步.
// 判据是本文件的常量; 完整扫描表随日志输出, 外设异常时可对照 truth table 定位.

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "bsp_board.h"
#include "bsp_i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "i2c"

static const char *TAG = "test_bsp_i2c";

#define SCAN_ADDR_CAPACITY (BSP_I2C_SCAN_LAST_ADDR - BSP_I2C_SCAN_FIRST_ADDR + 1)
#define SCAN_TIMEOUT_MS BSP_I2C_SCAN_DEFAULT_TIMEOUT_MS

typedef struct {
    uint8_t address;
    const char *name;
} expected_device_t;

// truth table 真机确认过的地址; 按板分开, 与文档保持同一事实.
static const expected_device_t AURAS3_DEVICES[] = {
    {0x18, "ES8311"},
    {0x20, "TCA9554PWR"},
    {0x34, "AXP2101"},
    {0x40, "ES7210"},
    {0x51, "PCF85063"},
    {0x5A, "CST9217"},
    {0x6B, "QMI8658"},
};

// DoerS3 GC0308 的 SCCB 也在这条总线上, 地址未进 truth table, 不列入期望.
static const expected_device_t DOERS3_DEVICES[] = {
    {0x18, "ES8311"},
    {0x19, "PCA9557"},
    {0x38, "FT6X36"},
    {0x41, "ES7210"},
    {0x6A, "QMI8658"},
};

static uint8_t s_scan[SCAN_ADDR_CAPACITY];
static size_t s_scan_count;
static bool s_scan_cached;

void setUp(void)
{
}

// 用例中途失败可能留下未配对的 acquire; 这里收干引用计数, 保证用例隔离.
void tearDown(void)
{
    while (bsp_i2c_release() == ESP_OK) {
    }
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

static void scan_cache(void)
{
    if (s_scan_cached) {
        return;
    }
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_scan(s_scan, SCAN_ADDR_CAPACITY, &s_scan_count, SCAN_TIMEOUT_MS));
    TEST_ASSERT_TRUE_MESSAGE(s_scan_count > 0, "no I2C device found on the board bus");
    s_scan_cached = true;
}

static const expected_device_t *expected_devices(size_t *count_out)
{
    const bsp_board_info_t *board = bsp_board_get_info();
    if (board != NULL) {
        if (board->id == BSP_BOARD_ID_AURAS3) {
            *count_out = sizeof(AURAS3_DEVICES) / sizeof(AURAS3_DEVICES[0]);
            return AURAS3_DEVICES;
        }
        if (board->id == BSP_BOARD_ID_DOERS3) {
            *count_out = sizeof(DOERS3_DEVICES) / sizeof(DOERS3_DEVICES[0]);
            return DOERS3_DEVICES;
        }
    }
    *count_out = 0;
    return NULL;
}

TEST_CASE("i2c: probe validates arguments and reports missing devices", "[i2c]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_i2c_probe(BSP_I2C_SCAN_FIRST_ADDR - 1, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_i2c_probe(BSP_I2C_SCAN_LAST_ADDR + 1, 0));

    scan_cache();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_probe(s_scan[0], 0));

    // 从扫描空位里挑一个地址: 必须回报 NOT_FOUND, 而不是误报在线.
    int missing = -1;
    for (uint8_t addr = BSP_I2C_SCAN_FIRST_ADDR; addr <= BSP_I2C_SCAN_LAST_ADDR; addr++) {
        if (!address_found(addr, s_scan, s_scan_count)) {
            missing = addr;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(missing > 0, "scan found every address; nothing left to probe as missing");
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, bsp_i2c_probe((uint8_t)missing, 0));
}

TEST_CASE("i2c: scan reports devices and honors buffer semantics", "[i2c]")
{
    size_t count = 0;
    uint8_t buffer[SCAN_ADDR_CAPACITY] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_i2c_scan(NULL, 1, &count, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_i2c_scan(buffer, 1, NULL, 0));

    size_t full_count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_scan(buffer, SCAN_ADDR_CAPACITY, &full_count, SCAN_TIMEOUT_MS));
    TEST_ASSERT_TRUE_MESSAGE(full_count > 0, "no I2C device found on the board bus");

    // 完整扫描表 (诊断输出): 外设异常时可对照 truth table.
    ESP_LOGI(TAG, "bus scan: %u device(s)", (unsigned)full_count);
    ESP_LOGI(TAG, "     00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f");
    for (uint8_t row = 0; row <= 0x70; row += 0x10) {
        char line[80];
        int pos = snprintf(line, sizeof(line), "%02x:", row);
        for (uint8_t col = 0; col < 0x10; col++) {
            const uint8_t addr = row + col;
            if (addr < BSP_I2C_SCAN_FIRST_ADDR || addr > BSP_I2C_SCAN_LAST_ADDR) {
                pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "   ");
            } else if (address_found(addr, buffer, full_count)) {
                pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " %02x", addr);
            } else {
                pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " --");
            }
        }
        ESP_LOGI(TAG, "%s", line);
    }

    // 小缓冲: 结果装不下时返回 INVALID_SIZE, 但 count 仍是完整数量, 且已填入前面的设备.
    uint8_t one = 0;
    size_t one_count = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, bsp_i2c_scan(&one, 1, &one_count, SCAN_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)full_count, (uint32_t)one_count);
    TEST_ASSERT_EQUAL_UINT8(buffer[0], one);

    // capacity = 0 + NULL: 只计数; 设备数大于 0 时同样报 INVALID_SIZE.
    size_t count_only = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, bsp_i2c_scan(NULL, 0, &count_only, 0));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)full_count, (uint32_t)count_only);

    // 重复扫描结果一致: 总线稳定, 无漏检.
    uint8_t again[SCAN_ADDR_CAPACITY] = {0};
    size_t again_count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_scan(again, SCAN_ADDR_CAPACITY, &again_count, SCAN_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)full_count, (uint32_t)again_count);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(buffer, again, full_count);
}

TEST_CASE("i2c: known devices are present on the bus", "[i2c]")
{
    size_t device_count = 0;
    const expected_device_t *devices = expected_devices(&device_count);
    if (devices == NULL) {
        TEST_IGNORE_MESSAGE("board has no expected-device table yet");
    }

    scan_cache();

    size_t missing = 0;
    ESP_LOGI(TAG, "expected devices (%u):", (unsigned)device_count);
    for (size_t i = 0; i < device_count; i++) {
        const bool present = address_found(devices[i].address, s_scan, s_scan_count);
        ESP_LOGI(TAG, "  0x%02x %-12s %s", devices[i].address, devices[i].name, present ? "present" : "MISSING");
        if (!present) {
            missing++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)missing, "expected devices missing from the bus");

    // 表外的地址只提示不算失败: 可能对应新增硬件或其他总线功能.
    for (size_t i = 0; i < s_scan_count; i++) {
        bool known = false;
        for (size_t j = 0; j < device_count; j++) {
            if (devices[j].address == s_scan[i]) {
                known = true;
                break;
            }
        }
        if (!known) {
            ESP_LOGW(TAG, "  0x%02x found on bus but not in the expected table", s_scan[i]);
        }
    }
}

TEST_CASE("i2c: acquire is reference counted and release is guarded", "[i2c]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_i2c_release());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_i2c_acquire(NULL));

    i2c_master_bus_handle_t first = NULL;
    i2c_master_bus_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_acquire(&first));
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_acquire(&second));
    TEST_ASSERT_EQUAL_PTR(first, second);

    // 参考计数: 释放一层后总线仍可用.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_release());
    scan_cache();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_probe(s_scan[0], 0));

    // 归零后再释放报 INVALID_STATE.
    TEST_ASSERT_EQUAL(ESP_OK, bsp_i2c_release());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_i2c_release());
}

void app_main(void)
{
    selftest_start(MODULE);
    selftest_run(MODULE);
}

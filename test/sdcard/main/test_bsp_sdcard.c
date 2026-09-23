// test/sdcard: bsp_sdcard 板级自检. 只使用 BSP public API; 判据是本文件的常量.

#include <stdio.h>
#include <string.h>

#include "bsp_sdcard.h"
#include "esp_err.h"
#include "esp_log.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "sdcard"

static const char *TAG = "test_bsp_sdcard";

// 挂载点由本测试选定, 与 board port 无关.
#define TEST_MOUNT_POINT "/sdcard"
#define TEST_FILE_PATH TEST_MOUNT_POINT "/selftest_sdcard.bin"
// 能挂上 FAT 的卡至少应达到 1 MB.
#define MIN_CAPACITY_BYTES (1024ULL * 1024ULL)
// 写读回比对的字节数. 缓冲必须放 static: app_main 任务栈只有几 KB,
// 把这些字节放栈上会在函数序言阶段就越界, 冲掉堆锁并触发 interrupt wdt.
#define ROUND_TRIP_BYTES 4096
// 32 字节挂载点 (含前导 '/') 是非法值.
#define TOO_LONG_MOUNT_POINT "/0123456789012345678901234567890"

static bsp_sdcard_handle_t s_sdcard;

void setUp(void)
{
}

void tearDown(void)
{
    if (s_sdcard != NULL) {
        (void)bsp_sdcard_close(s_sdcard);
        s_sdcard = NULL;
    }
}

static void open_sdcard(void)
{
    s_sdcard = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_open(&s_sdcard));
    TEST_ASSERT_NOT_NULL(s_sdcard);
}

static void mount_sdcard(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_mount(s_sdcard, TEST_MOUNT_POINT));
}

TEST_CASE("sdcard: open and close", "[sdcard]")
{
    open_sdcard();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_close(s_sdcard));
    s_sdcard = NULL;
}

TEST_CASE("sdcard: second open rejected until closed", "[sdcard]")
{
    open_sdcard();

    bsp_sdcard_handle_t second = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_sdcard_open(&second));

    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_close(s_sdcard));
    s_sdcard = NULL;

    open_sdcard();
}

TEST_CASE("sdcard: null arguments rejected", "[sdcard]")
{
    bsp_sdcard_info_t info = {0};
    bsp_sdcard_fs_info_t fs = {0};
    const char *mount_point = NULL;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_open(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_close(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_unmount(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_get_info(NULL, &info));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_get_fs_info(NULL, &fs));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_get_mount_point(NULL, &mount_point));

    open_sdcard();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_mount(s_sdcard, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_get_info(s_sdcard, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_get_fs_info(s_sdcard, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_get_mount_point(s_sdcard, NULL));
}

TEST_CASE("sdcard: info reports not mounted before mount", "[sdcard]")
{
    open_sdcard();

    bsp_sdcard_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_get_info(s_sdcard, &info));
    TEST_ASSERT_FALSE(info.mounted);

    bsp_sdcard_fs_info_t fs = {0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_get_fs_info(s_sdcard, &fs));
    TEST_ASSERT_FALSE(fs.mounted);

    const char *mount_point = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_sdcard_get_mount_point(s_sdcard, &mount_point));
}

TEST_CASE("sdcard: mount point validation", "[sdcard]")
{
    open_sdcard();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_mount(s_sdcard, ""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_mount(s_sdcard, "sdcard"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_sdcard_mount(s_sdcard, TOO_LONG_MOUNT_POINT));

    const char *mount_point = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_sdcard_get_mount_point(s_sdcard, &mount_point));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_sdcard_unmount(s_sdcard));
}

TEST_CASE("sdcard: mount and unmount", "[sdcard]")
{
    open_sdcard();
    mount_sdcard();

    const char *mount_point = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_get_mount_point(s_sdcard, &mount_point));
    TEST_ASSERT_EQUAL_STRING(TEST_MOUNT_POINT, mount_point);

    bsp_sdcard_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_get_info(s_sdcard, &info));
    ESP_LOGI(TAG, "card: name=%s type=%d capacity=%.1f MB sectors=%llu sector_size=%u bus_width=%u real_freq=%d kHz",
             info.name,
             (int)info.type,
             (double)info.capacity_bytes / (1024.0 * 1024.0),
             (unsigned long long)info.sector_count,
             (unsigned)info.sector_size,
             (unsigned)info.bus_width,
             info.real_freq_khz);

    TEST_ASSERT_TRUE(info.mounted);
    TEST_ASSERT_TRUE(info.name[0] != '\0');
    TEST_ASSERT_TRUE(info.type != BSP_SDCARD_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(info.capacity_bytes >= MIN_CAPACITY_BYTES);
    TEST_ASSERT_TRUE(info.sector_count > 0);
    TEST_ASSERT_EQUAL_UINT32(512, info.sector_size);
    TEST_ASSERT_EQUAL_UINT8(1, info.bus_width);
    TEST_ASSERT_TRUE(info.real_freq_khz > 0);

    bsp_sdcard_fs_info_t fs = {0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_get_fs_info(s_sdcard, &fs));
    ESP_LOGI(TAG, "fs: total=%.1f MB free=%.1f MB",
             (double)fs.total_bytes / (1024.0 * 1024.0),
             (double)fs.free_bytes / (1024.0 * 1024.0));

    TEST_ASSERT_TRUE(fs.mounted);
    TEST_ASSERT_TRUE(fs.total_bytes >= MIN_CAPACITY_BYTES);
    TEST_ASSERT_TRUE(fs.free_bytes <= fs.total_bytes);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_unmount(s_sdcard));

    memset(&info, 0xAA, sizeof(info));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_get_info(s_sdcard, &info));
    TEST_ASSERT_FALSE(info.mounted);
}

TEST_CASE("sdcard: file write and read round trip", "[sdcard]")
{
    open_sdcard();
    mount_sdcard();

    static uint8_t written[ROUND_TRIP_BYTES];
    static uint8_t readback[ROUND_TRIP_BYTES];

    for (size_t i = 0; i < sizeof(written); i++) {
        written[i] = (uint8_t)(i * 31u + 7u);
    }
    memset(readback, 0, sizeof(readback));

    FILE *file = fopen(TEST_FILE_PATH, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_UINT(sizeof(written), fwrite(written, 1, sizeof(written), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    file = fopen(TEST_FILE_PATH, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_UINT(sizeof(readback), fread(readback, 1, sizeof(readback), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    TEST_ASSERT_EQUAL_MEMORY(written, readback, sizeof(written));
    ESP_LOGI(TAG, "wrote and read back %u bytes at %s", (unsigned)sizeof(written), TEST_FILE_PATH);

    TEST_ASSERT_EQUAL_INT(0, remove(TEST_FILE_PATH));
    TEST_ASSERT_NULL(fopen(TEST_FILE_PATH, "rb"));
}

TEST_CASE("sdcard: close unmounts", "[sdcard]")
{
    open_sdcard();
    mount_sdcard();

    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_close(s_sdcard));
    s_sdcard = NULL;

    open_sdcard();
    bsp_sdcard_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_sdcard_get_info(s_sdcard, &info));
    TEST_ASSERT_FALSE(info.mounted);
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_sdcard_desc_t *desc = bsp_sdcard_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no sdcard on this board");
        return;
    }

    selftest_run(MODULE);
}

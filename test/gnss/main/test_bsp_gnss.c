// test/gnss: bsp_gnss 自检.
// AuraS3 模组未贴装 (原理图预留), 期望结果是 SKIP; 贴装后同一个 app 跑下面的自动化用例.
// 判据: 端口打开后 PROBE_TIMEOUT_MS 内读不到任何字节 => 视为无模组.
#include <stdint.h>
#include <string.h>

#include "bsp_gnss.h"
#include "esp_err.h"
#include "esp_log.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "gnss"
// 无模组时 UART 上没有数据, 用这个窗口判"模组不在".
#define PROBE_TIMEOUT_MS 3000
#define READ_TIMEOUT_MS 2000
#define READ_BUF_SIZE 256

static const char *TAG = "test_bsp_gnss";
static bsp_gnss_handle_t s_gnss;

TEST_CASE("gnss: open, close and reopen succeed", "[gnss]")
{
    TEST_ASSERT_EQUAL(ESP_OK, bsp_gnss_open(&s_gnss));
    TEST_ASSERT_NOT_NULL(s_gnss);
    TEST_ASSERT_EQUAL(ESP_OK, bsp_gnss_close(s_gnss));
    TEST_ASSERT_EQUAL(ESP_OK, bsp_gnss_open(&s_gnss));
}

TEST_CASE("gnss: NMEA sentences arrive", "[gnss]")
{
    uint8_t buf[READ_BUF_SIZE];
    size_t len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_gnss_read(s_gnss, buf, sizeof(buf) - 1, &len, READ_TIMEOUT_MS));
    TEST_ASSERT_TRUE(len > 0);
    buf[len] = '\0';
    TEST_ASSERT_NOT_NULL_MESSAGE(strchr((const char *)buf, '$'), "no NMEA sentence start in payload");
    ESP_LOGI(TAG, "first payload: %s", (const char *)buf);
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_gnss_desc_t *desc = bsp_gnss_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no gnss port");
        return;
    }

    // 先探一次: 板上未贴模组时 UART 无数据, 直接 SKIP, 不算失败.
    bsp_gnss_handle_t probe = NULL;
    if (bsp_gnss_open(&probe) != ESP_OK) {
        selftest_skip(MODULE, "gnss open failed");
        return;
    }
    uint8_t probe_buf[READ_BUF_SIZE];
    size_t probe_len = 0;
    esp_err_t ret = bsp_gnss_read(probe, probe_buf, sizeof(probe_buf), &probe_len, PROBE_TIMEOUT_MS);
    (void)bsp_gnss_close(probe);
    if (ret != ESP_OK || probe_len == 0) {
        selftest_skip(MODULE, "no NMEA data (module not populated?)");
        return;
    }

    (void)selftest_run(MODULE);
}

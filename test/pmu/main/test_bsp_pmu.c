// test/pmu: bsp_pmu 板级自检. 只使用 BSP public API; 判据是本文件的常量.
// 电池相关项在没有电池时记为 pending; 软件关机是破坏性人工动作, 放在汇总行之后
// 由操作者触发, 行为结论写进 docs/hw/auras3-pmu-key.md 和对应 truth table.
// VBUS_INSERT/REMOVE 事件不在覆盖范围: USB 串口同时供电和出日志, 制造不了 VBUS
// 拔插, 需要电池供电 + 独立日志通道的台位; 缺口记录在 docs/bsp/status.md.

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "bsp_pmu.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "selftest.h"
#include "unity.h"

#define MODULE "pmu"

static const char *TAG = "test_bsp_pmu";

// 事件轮询间隔.
#define POLL_MS 50
// 引导人工动作 (短按 KEY2, 拔插电池) 的等待上限.
#define GUIDED_TIMEOUT_MS 30000
// 清空事件后再次读取前的静置窗口.
#define CLEAR_SETTLE_MS 200
// AXP2101 的 ADC 是低速 SAR: 通道使能后首次转换完成前, 数据寄存器读 0.
// 刚 open 就读取时必须轮询等待, 不能立刻断言.
#define ADC_SETTLE_TIMEOUT_MS 5000
#define ADC_SETTLE_POLL_MS 100

// 状态字段的合理区间; 越界说明映射或换算错了, 不是精度问题.
#define SYSTEM_MV_MIN 2500
#define SYSTEM_MV_MAX 5000
#define VBUS_MV_MIN 4000
#define VBUS_MV_MAX 6000
#define BATTERY_MV_MIN 2500
#define BATTERY_MV_MAX 4600
#define TEMPERATURE_C_MIN (-40.0f)
#define TEMPERATURE_C_MAX 125.0f

static bsp_pmu_handle_t s_pmu;

void setUp(void)
{
}

void tearDown(void)
{
    if (s_pmu != NULL) {
        (void)bsp_pmu_close(s_pmu);
        s_pmu = NULL;
    }
}

static void open_pmu(void)
{
    s_pmu = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_open(&s_pmu));
    TEST_ASSERT_NOT_NULL(s_pmu);
}

// 轮询并消费事件, 直到出现 want; 只用于引导项. seen_out 回传窗口内实际
// 见过的事件位, 超时时用于区分"没按"和"按了但事件类型不同".
static bool wait_for_event(bsp_pmu_event_t want, int timeout_ms, bsp_pmu_event_t *seen_out)
{
    bsp_pmu_event_t seen = BSP_PMU_EVENT_NONE;
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline_us) {
        bsp_pmu_event_t events = BSP_PMU_EVENT_NONE;
        TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_get_events(s_pmu, &events, true));
        seen |= events;
        if ((events & want) != 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    if (seen_out != NULL) {
        *seen_out = seen;
    }
    return (seen & want) != 0;
}

// 首次转换完成前 ADC 数据寄存器读 0; 所有 ADC 派生字段都越过 0 值才算收敛.
static bool adc_readings_sane(const bsp_pmu_status_t *status)
{
    return status->system_voltage_mv >= SYSTEM_MV_MIN && status->system_voltage_mv <= SYSTEM_MV_MAX &&
           status->pmu_temperature_c > TEMPERATURE_C_MIN && status->pmu_temperature_c < TEMPERATURE_C_MAX &&
           (!status->vbus_good || status->vbus_voltage_mv >= VBUS_MV_MIN);
}

// 轮询 status 直到 ADC 读数合理, 返回是否收敛.
static bool wait_for_adc_settle(bsp_pmu_status_t *status_out)
{
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)ADC_SETTLE_TIMEOUT_MS * 1000;
    while (true) {
        bsp_pmu_status_t status = {0};
        TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_get_status(s_pmu, &status));
        const bool settled = adc_readings_sane(&status);
        if (settled || esp_timer_get_time() >= deadline_us) {
            *status_out = status;
            return settled;
        }
        vTaskDelay(pdMS_TO_TICKS(ADC_SETTLE_POLL_MS));
    }
}

TEST_CASE("pmu: desc is sane and open/close works", "[pmu]")
{
    const bsp_pmu_desc_t *desc = bsp_pmu_get_desc();
    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_TRUE(desc->present);
    TEST_ASSERT_NOT_NULL(desc->model);
    TEST_ASSERT_TRUE(desc->model[0] != '\0');
    ESP_LOGI(TAG, "model: %s", desc->model);

    open_pmu();
    TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_close(s_pmu));
    s_pmu = NULL;
}

TEST_CASE("pmu: second open rejected until closed", "[pmu]")
{
    open_pmu();

    bsp_pmu_handle_t second = (bsp_pmu_handle_t)(uintptr_t)0x1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, bsp_pmu_open(&second));
    TEST_ASSERT_NULL(second);

    TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_close(s_pmu));
    s_pmu = NULL;

    open_pmu();
}

TEST_CASE("pmu: null arguments rejected", "[pmu]")
{
    bsp_pmu_status_t status = {0};
    bsp_pmu_event_t events = BSP_PMU_EVENT_NONE;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_pmu_open(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_pmu_close(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_pmu_get_status(NULL, &status));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_pmu_get_events(NULL, &events, false));
    // 依赖契约"先校验 handle, 再做任何副作用": 实现必须把校验保持在最前,
    // 否则这条用例会真的断板子的电.
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_pmu_power_off(NULL));

    open_pmu();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_pmu_get_status(s_pmu, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bsp_pmu_get_events(s_pmu, NULL, false));
}

TEST_CASE("pmu: status is structurally sane", "[pmu]")
{
    open_pmu();

    bsp_pmu_status_t status = {0};
    const bool settled = wait_for_adc_settle(&status);

    ESP_LOGI(TAG, "vbus: good=%d voltage=%dmV", (int)status.vbus_good, status.vbus_voltage_mv);
    ESP_LOGI(TAG, "battery: present=%d percent=%d voltage=%dmV", (int)status.battery_present,
             status.battery_percent, status.battery_voltage_mv);
    ESP_LOGI(TAG, "power: state=%d charge_state=%d", (int)status.power_state, (int)status.charge_state);
    ESP_LOGI(TAG, "system: voltage=%dmV temperature=%.1fC", status.system_voltage_mv, status.pmu_temperature_c);

    TEST_ASSERT_TRUE_MESSAGE(settled, "VSYS ADC never produced a reading after open");
    TEST_ASSERT_TRUE(status.power_state >= BSP_PMU_POWER_STATE_UNKNOWN &&
                     status.power_state <= BSP_PMU_POWER_STATE_DISCHARGING);
    TEST_ASSERT_TRUE(status.charge_state >= BSP_PMU_CHARGE_STATE_UNKNOWN &&
                     status.charge_state <= BSP_PMU_CHARGE_STATE_NOT_CHARGING);
    TEST_ASSERT_TRUE(status.system_voltage_mv >= SYSTEM_MV_MIN && status.system_voltage_mv <= SYSTEM_MV_MAX);
    TEST_ASSERT_TRUE(status.pmu_temperature_c > TEMPERATURE_C_MIN && status.pmu_temperature_c < TEMPERATURE_C_MAX);

    if (status.vbus_good) {
        TEST_ASSERT_TRUE(status.vbus_voltage_mv >= VBUS_MV_MIN && status.vbus_voltage_mv <= VBUS_MV_MAX);
    } else {
        TEST_ASSERT_EQUAL_INT(-1, status.vbus_voltage_mv);
    }

    if (status.battery_present) {
        TEST_ASSERT_TRUE(status.battery_voltage_mv >= BATTERY_MV_MIN && status.battery_voltage_mv <= BATTERY_MV_MAX);
        TEST_ASSERT_TRUE(status.battery_percent >= 0 && status.battery_percent <= 100);
    } else {
        TEST_ASSERT_EQUAL_INT(-1, status.battery_percent);
        TEST_ASSERT_EQUAL_INT(-1, status.battery_voltage_mv);
    }
}

TEST_CASE("pmu: clear consumes the delivered events", "[pmu]")
{
    open_pmu();

    bsp_pmu_event_t first = BSP_PMU_EVENT_NONE;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_get_events(s_pmu, &first, true));
    ESP_LOGI(TAG, "events right after open (cleared): 0x%08" PRIx32, (uint32_t)first);

    vTaskDelay(pdMS_TO_TICKS(CLEAR_SETTLE_MS));

    bsp_pmu_event_t second = BSP_PMU_EVENT_NONE;
    TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_get_events(s_pmu, &second, true));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(BSP_PMU_EVENT_NONE, (uint32_t)second, "cleared events were re-reported");
}

TEST_CASE("pmu: power key short press is reported", "[pmu]")
{
    open_pmu();

    ESP_LOGI(TAG, ">>> short-press KEY2 once; do NOT long-press (long press cuts power)");
    bsp_pmu_event_t seen = BSP_PMU_EVENT_NONE;
    const bool got = wait_for_event(BSP_PMU_EVENT_POWER_KEY_SHORT, GUIDED_TIMEOUT_MS, &seen);
    if (!got) {
        ESP_LOGE(TAG, "guided window saw events: 0x%08" PRIx32, (uint32_t)seen);
    }
    TEST_ASSERT_TRUE_MESSAGE(got, "no POWER_KEY_SHORT event after the guided short press");
}

TEST_CASE("pmu: battery insert and remove are reported", "[pmu]")
{
    open_pmu();

    bsp_pmu_status_t status = {0};
    TEST_ASSERT_EQUAL(ESP_OK, bsp_pmu_get_status(s_pmu, &status));
    if (!status.battery_present) {
        TEST_IGNORE_MESSAGE("no battery attached: insert/remove events need a battery");
    }

    ESP_LOGI(TAG, ">>> unplug the battery now, then plug it back");
    TEST_ASSERT_TRUE_MESSAGE(wait_for_event(BSP_PMU_EVENT_BATTERY_REMOVE, GUIDED_TIMEOUT_MS, NULL),
                             "no BATTERY_REMOVE event after unplugging the battery");
    TEST_ASSERT_TRUE_MESSAGE(wait_for_event(BSP_PMU_EVENT_BATTERY_INSERT, GUIDED_TIMEOUT_MS, NULL),
                             "no BATTERY_INSERT event after plugging the battery back");
}

void app_main(void)
{
    selftest_start(MODULE);

    const bsp_pmu_desc_t *desc = bsp_pmu_get_desc();
    if (desc == NULL || !desc->present) {
        selftest_skip(MODULE, "no pmu on this board");
        return;
    }

    selftest_run(MODULE);

    // 破坏性人工验证: 断电成功时本次运行不会再有输出, 属于预期; 这一步不进判据,
    // 行为结论 (USB / 仅电池 / 双供, KEY2 重新开机) 记录在 docs/hw/auras3-pmu-key.md.
    const int answer = selftest_ask_yes_no(
        "run the power-off check now? y = the board cuts power (press KEY2 to power it back)", GUIDED_TIMEOUT_MS);
    if (answer != 1) {
        ESP_LOGI(TAG, "power-off check skipped");
        return;
    }

    bsp_pmu_handle_t pmu = NULL;
    if (bsp_pmu_open(&pmu) != ESP_OK) {
        ESP_LOGE(TAG, "power-off check: open failed");
        return;
    }

    ESP_LOGW(TAG, ">>> cutting power now; if the rails cut, this is the last line");
    const esp_err_t ret = bsp_pmu_power_off(pmu);
    ESP_LOGW(TAG, "power-off check: still running after power_off (%s); record the observed behavior",
             esp_err_to_name(ret));
    vTaskDelay(pdMS_TO_TICKS(1000));
    (void)bsp_pmu_close(pmu);
}

#include <stdio.h>

#include "bsp_board.h"
#include "bsp_pmu.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "test_bsp_pmu"
#define PMU_OBSERVE_COUNT 200
#define PMU_OBSERVE_INTERVAL_MS 500

static const char *charge_state_name(bsp_pmu_charge_state_t state)
{
    switch (state) {
    case BSP_PMU_CHARGE_STATE_TRICKLE:
        return "trickle";
    case BSP_PMU_CHARGE_STATE_PRECHARGE:
        return "precharge";
    case BSP_PMU_CHARGE_STATE_CONSTANT_CURRENT:
        return "constant_current";
    case BSP_PMU_CHARGE_STATE_CONSTANT_VOLTAGE:
        return "constant_voltage";
    case BSP_PMU_CHARGE_STATE_DONE:
        return "done";
    case BSP_PMU_CHARGE_STATE_NOT_CHARGING:
        return "not_charging";
    case BSP_PMU_CHARGE_STATE_UNKNOWN:
    default:
        return "unknown";
    }
}

static void print_events(bsp_pmu_event_t events)
{
    printf("events      : 0x%08lx\n", (unsigned long)events);
    printf("  vbus      : insert=%d remove=%d\n",
           (events & BSP_PMU_EVENT_VBUS_INSERT) != 0,
           (events & BSP_PMU_EVENT_VBUS_REMOVE) != 0);
    printf("  battery   : insert=%d remove=%d\n",
           (events & BSP_PMU_EVENT_BATTERY_INSERT) != 0,
           (events & BSP_PMU_EVENT_BATTERY_REMOVE) != 0);
    printf("  charge    : start=%d done=%d\n",
           (events & BSP_PMU_EVENT_CHARGE_START) != 0,
           (events & BSP_PMU_EVENT_CHARGE_DONE) != 0);
    printf("  pkey      : short=%d long=%d\n",
           (events & BSP_PMU_EVENT_POWER_KEY_SHORT) != 0,
           (events & BSP_PMU_EVENT_POWER_KEY_LONG) != 0);
}

static void print_status_summary(const bsp_pmu_status_t *status)
{
    printf("vbus        : present=%d good=%d voltage=%dmV\n",
           status->vbus_present, status->vbus_good, status->vbus_voltage_mv);
    printf("battery     : present=%d percent=%d voltage=%dmV\n",
           status->battery_present, status->battery_percent, status->battery_voltage_mv);
    printf("charge      : charging=%d discharging=%d standby=%d state=%s\n",
           status->charging, status->discharging, status->standby,
           charge_state_name(status->charge_state));
    printf("system      : voltage=%dmV\n", status->system_voltage_mv);
    printf("temperature : %.2fC\n", status->pmu_temperature_c);
}

void app_main(void)
{
    ESP_LOGI(TAG, "TEST START name=pmu");

    const bsp_board_info_t *board = bsp_board_get_info();
    printf("board       : %s\n", board->name);
    const bsp_pmu_desc_t *pmu_desc = bsp_pmu_get_desc();
    if (!pmu_desc->present) {
        ESP_LOGI(TAG, "TEST SKIP name=pmu summary=\"pmu unsupported\"");
        return;
    }

    printf("pmu         : present=%d model=%s\n", pmu_desc->present, pmu_desc->model == NULL ? "" : pmu_desc->model);

    bsp_pmu_handle_t pmu = NULL;
    ESP_ERROR_CHECK(bsp_pmu_open(NULL, &pmu));

    bsp_pmu_status_t status = {0};
    ESP_ERROR_CHECK(bsp_pmu_get_status(pmu, &status));
    print_status_summary(&status);

    bsp_pmu_event_t events = BSP_PMU_EVENT_NONE;
    ESP_ERROR_CHECK(bsp_pmu_get_events(pmu, &events, true));
    print_events(events);

    printf("\nobserve     : %d samples, interval=%dms\n", PMU_OBSERVE_COUNT, PMU_OBSERVE_INTERVAL_MS);
    printf("hint        : short-press and long-press KEY2 while samples are printing\n");
    printf("columns     : %3s %4s %3s %3s %7s %4s %5s %10s\n",
           "idx", "vbus", "bat", "chg", "standby", "pct", "batmv", "events");

    for (int i = 0; i < PMU_OBSERVE_COUNT; ++i) {
        ESP_ERROR_CHECK(bsp_pmu_get_status(pmu, &status));
        ESP_ERROR_CHECK(bsp_pmu_get_events(pmu, &events, true));

        printf("sample      : %03d %4d %3d %3d %7d %4d %5d 0x%08lx\n",
               i,
               status.vbus_present,
               status.battery_present,
               status.charging,
               status.standby,
               status.battery_percent,
               status.battery_voltage_mv,
               (unsigned long)events);

        if (events != BSP_PMU_EVENT_NONE) {
            print_events(events);
        }

        vTaskDelay(pdMS_TO_TICKS(PMU_OBSERVE_INTERVAL_MS));
    }

    ESP_ERROR_CHECK(bsp_pmu_close(pmu));
    ESP_LOGI(TAG, "TEST PASS name=pmu summary=\"status and key observation printed\"");
}

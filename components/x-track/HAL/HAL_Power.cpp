#include "HAL.h"

namespace {

uint16_t sAutoLowPowerTimeoutSec = 0;
bool sAutoLowPowerEnable = false;
HAL::Power_CallbackFunction_t sPowerEventCb = nullptr;

}  // namespace

void HAL::Power_Init()
{
    sAutoLowPowerTimeoutSec = 0;
    sAutoLowPowerEnable = false;
}

void HAL::Power_HandleTimeUpdate()
{
}

void HAL::Power_SetAutoLowPowerTimeout(uint16_t sec)
{
    sAutoLowPowerTimeoutSec = sec;
}

uint16_t HAL::Power_GetAutoLowPowerTimeout()
{
    return sAutoLowPowerTimeoutSec;
}

void HAL::Power_SetAutoLowPowerEnable(bool en)
{
    sAutoLowPowerEnable = en;
}

void HAL::Power_Shutdown()
{
}

void HAL::Power_Update()
{
}

void HAL::Power_EventMonitor()
{
}

void HAL::Power_GetInfo(Power_Info_t *info)
{
    if (info == nullptr) {
        return;
    }

    info->voltage = 4200;
    info->usage = 100;
    info->isCharging = false;
}

void HAL::Power_SetEventCallback(Power_CallbackFunction_t callback)
{
    sPowerEventCb = callback;
}

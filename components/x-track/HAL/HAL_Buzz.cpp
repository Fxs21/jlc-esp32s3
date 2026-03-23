#include "HAL.h"

namespace {

bool sBuzzEnable = true;

}  // namespace

void HAL::Buzz_Init()
{

}

void HAL::Buzz_SetEnable(bool en)
{
    sBuzzEnable = en;
}

void HAL::Buzz_Tone(uint32_t freq, int32_t duration)
{
    (void)freq;
    (void)duration;
}

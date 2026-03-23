#pragma once

#include <math.h>
#include <stdint.h>

#include "esp_timer.h"

typedef uint8_t byte;

#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif

#ifndef radians
#define radians(deg) ((deg) * 0.017453292519943295769236907684886)
#endif

#ifndef degrees
#define degrees(rad) ((rad) * 57.295779513082320876798154814105)
#endif

#ifndef sq
#define sq(x) ((x) * (x))
#endif

static inline unsigned long millis(void)
{
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

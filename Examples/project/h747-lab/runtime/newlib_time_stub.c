#include <sys/time.h>

#include "stm32h7xx_hal.h"

int _gettimeofday(struct timeval* tv, void* tzvp) {
    (void)tzvp;
    if (tv == 0) {
        return 0;
    }
    const unsigned long ms = HAL_GetTick();
    tv->tv_sec = (time_t)(ms / 1000UL);
    tv->tv_usec = (suseconds_t)((ms % 1000UL) * 1000UL);
    return 0;
}

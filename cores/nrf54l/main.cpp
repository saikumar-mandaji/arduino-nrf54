/**
 * @file main.cpp
 * @brief Sketch entry point. Reset_Handler (see extern/nrfx's MDK startup
 *        assembly) calls SystemInit() then jumps to the C runtime's _start,
 *        which runs static constructors and calls this main().
 */
#include "Arduino.h"

extern "C" void nrf54_core_time_init(void);

void nrf54_core_init(void)
{
    nrf54_core_time_init();
}

int main(void)
{
    nrf54_core_init();
    setup();
    while (1)
    {
        loop();
    }
    return 0;
}

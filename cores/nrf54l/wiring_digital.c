/**
 * @file wiring_digital.c
 * @brief pinMode/digitalWrite/digitalRead on top of nrfx's nrf_gpio HAL.
 *
 * Deliberately built on the direct nrf_gpio.h register HAL rather than
 * nrfx_gpiote -- gpiote is for pin-change interrupts/PPI event routing,
 * which v1 of this core does not need. Plain nrf_gpio_* calls are enough
 * for synchronous digitalWrite/digitalRead/pinMode.
 */
#include "Arduino.h"
#include <hal/nrf_gpio.h>

void pinMode(uint32_t pin, uint32_t mode)
{
    switch (mode)
    {
        case OUTPUT:
            nrf_gpio_cfg_output(pin);
            break;
        case INPUT:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
            break;
        case INPUT_PULLUP:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
            break;
        case INPUT_PULLDOWN:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
            break;
        default:
            break;
    }
}

void digitalWrite(uint32_t pin, uint32_t value)
{
    if (value)
    {
        nrf_gpio_pin_set(pin);
    }
    else
    {
        nrf_gpio_pin_clear(pin);
    }
}

int digitalRead(uint32_t pin)
{
    return (int)nrf_gpio_pin_read(pin);
}

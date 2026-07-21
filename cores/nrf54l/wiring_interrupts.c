/**
 * @file wiring_interrupts.c
 * @brief attachInterrupt()/detachInterrupt() over GPIOTE.
 *
 * One shared dispatch handler is registered per pin (nrfx_gpiote's
 * handler_config is per-registration, not global); the real nRF54L15
 * GPIOTE pin/trigger is passed back into the dispatch function, which
 * looks the pin up in a small fixed table to find the user's callback.
 * See docs/ARCHITECTURE.md and docs/VERIFICATION.md.
 */
#include "Arduino.h"
#include <nrfx_gpiote.h>

#define MAX_INTERRUPT_PINS 8

static nrfx_gpiote_t s_gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);
static bool s_gpiote_began = false;

/* REAL BUG FOUND ON HARDWARE (2026-07-21), the most severe instance of
 * this class of bug: nrfx_gpiote's IRQ handler is generic/instance-
 * parameterized (nrfx_gpiote_irq_handler(nrfx_gpiote_t*)), same as
 * nrfx_uarte/nrfx_spim/nrfx_twim's (see the trampoline in
 * HardwareSerial.cpp for the full explanation). Unlike those, GPIOTE
 * has no blocking-mode fallback -- attachInterrupt() is interrupt-driven
 * by definition, so without this trampoline the real vector
 * (GPIOTE20_IRQHandler, which nrfx_mdk_fixups.h further resolves to
 * GPIOTE20_1_IRQHandler for this core's secure/non-TrustZone-split
 * build) stayed weak-aliased to Default_Handler and attachInterrupt()
 * callbacks could never fire at all, on any pin. */
void nrfx_gpiote_20_irq_handler(void)
{
    nrfx_gpiote_irq_handler(&s_gpiote);
}

typedef struct
{
    bool in_use;
    uint32_t pin;
    uint8_t channel;
    isr_callback_t callback;
} interrupt_slot_t;

static interrupt_slot_t s_slots[MAX_INTERRUPT_PINS];

static void gpiote_dispatch(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t trigger, void * p_context)
{
    (void)trigger;
    (void)p_context;

    for (int i = 0; i < MAX_INTERRUPT_PINS; i++)
    {
        if (s_slots[i].in_use && s_slots[i].pin == pin)
        {
            if (s_slots[i].callback)
            {
                s_slots[i].callback();
            }
            return;
        }
    }
}

static void gpiote_lazy_init(void)
{
    if (s_gpiote_began)
    {
        return;
    }
    if (nrfx_gpiote_init(&s_gpiote, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY) == 0)
    {
        s_gpiote_began = true;
    }
}

void attachInterrupt(uint32_t pin, isr_callback_t callback, int mode)
{
    gpiote_lazy_init();
    if (!s_gpiote_began || callback == NULL)
    {
        return;
    }

    /* Reattaching the same pin: detach first so we don't leak a channel
     * or leave a stale handler slot registered. */
    detachInterrupt(pin);

    int slot = -1;
    for (int i = 0; i < MAX_INTERRUPT_PINS; i++)
    {
        if (!s_slots[i].in_use)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        return; /* all MAX_INTERRUPT_PINS slots in use */
    }

    uint8_t channel;
    if (nrfx_gpiote_channel_alloc(&s_gpiote, &channel) != 0)
    {
        return;
    }

    nrfx_gpiote_trigger_t trigger;
    switch (mode)
    {
        case RISING:  trigger = NRFX_GPIOTE_TRIGGER_LOTOHI; break;
        case FALLING: trigger = NRFX_GPIOTE_TRIGGER_HITOLO; break;
        case CHANGE:  trigger = NRFX_GPIOTE_TRIGGER_TOGGLE; break;
        default:
            nrfx_gpiote_channel_free(&s_gpiote, channel);
            return;
    }

    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = trigger,
        .p_in_channel = &channel,
    };
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = gpiote_dispatch,
        .p_context = NULL,
    };
    /* p_pull_config is NULL: this core expects the sketch to have
     * already called pinMode(pin, INPUT/INPUT_PULLUP/INPUT_PULLDOWN)
     * before attachInterrupt(), matching standard Arduino practice --
     * we don't silently reconfigure the pull here. */
    nrfx_gpiote_input_pin_config_t input_config = {
        .p_pull_config = NULL,
        .p_trigger_config = &trigger_config,
        .p_handler_config = &handler_config,
    };

    if (nrfx_gpiote_input_configure(&s_gpiote, pin, &input_config) != 0)
    {
        nrfx_gpiote_channel_free(&s_gpiote, channel);
        return;
    }

    nrfx_gpiote_trigger_enable(&s_gpiote, pin, true);

    s_slots[slot].in_use = true;
    s_slots[slot].pin = pin;
    s_slots[slot].channel = channel;
    s_slots[slot].callback = callback;
}

void detachInterrupt(uint32_t pin)
{
    if (!s_gpiote_began)
    {
        return;
    }

    for (int i = 0; i < MAX_INTERRUPT_PINS; i++)
    {
        if (s_slots[i].in_use && s_slots[i].pin == pin)
        {
            nrfx_gpiote_trigger_disable(&s_gpiote, pin);
            nrfx_gpiote_channel_free(&s_gpiote, s_slots[i].channel);
            s_slots[i].in_use = false;
            s_slots[i].callback = NULL;
            return;
        }
    }
}

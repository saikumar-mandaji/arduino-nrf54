/**
 * @file wiring_analog.c
 * @brief analogRead() over SAADC (single-ended, 12-bit, blocking) and
 *        analogWrite() over PWM20 (4 fixed channels, individual load
 *        mode -- each of PIN_PWM0..PIN_PWM3 gets an independent duty
 *        cycle). See docs/ARCHITECTURE.md and docs/VERIFICATION.md.
 */
#include "Arduino.h"
#include <nrfx_saadc.h>
#include <nrfx_pwm.h>

/* ---- analogRead() / SAADC ---- */

static bool s_saadc_began = false;

static void saadc_lazy_init(void)
{
    if (s_saadc_began)
    {
        return;
    }
    if (nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY) == 0)
    {
        s_saadc_began = true;
    }
}

int analogRead(uint32_t pin)
{
    saadc_lazy_init();
    if (!s_saadc_began || pin > 7)
    {
        return -1;
    }

    /* NRFX_ANALOG_EXTERNAL_AIN0 (see extern/nrfx/helpers/nrfx_analog_common.h)
     * is nrfx's chip-agnostic analog-input enum -- the nRF54L15's SAADC
     * uses NRF_SAADC_HAS_AIN_AS_PIN-style pin selection under the hood
     * (confirmed via extern/nrfx/hal/nrf_saadc.h), so the older
     * NRF_SAADC_INPUT_AIN0-style chip-specific enum from nrf_saadc.h is
     * not defined for this chip at all -- this is the real, current API. */
    nrfx_saadc_channel_t channel = NRFX_SAADC_DEFAULT_CHANNEL_SE(NRFX_ANALOG_EXTERNAL_AIN0 + pin, 0);
    if (nrfx_saadc_channels_config(&channel, 1) != 0)
    {
        return -1;
    }

    /* NULL event_handler -> nrfx_saadc_mode_trigger() below is blocking
     * (see nrfx_saadc.h's nrfx_saadc_simple_mode_set() docs). */
    if (nrfx_saadc_simple_mode_set((1UL << 0), NRF_SAADC_RESOLUTION_12BIT, NRF_SAADC_OVERSAMPLE_DISABLED, NULL) != 0)
    {
        return -1;
    }

    nrf_saadc_value_t sample = 0;
    if (nrfx_saadc_buffer_set(&sample, 1) != 0)
    {
        return -1;
    }

    if (nrfx_saadc_mode_trigger() != 0)
    {
        return -1;
    }

    return (int)sample;
}

/* ---- analogWrite() / PWM20 ---- */

static nrfx_pwm_t s_pwm = NRFX_PWM_INSTANCE(NRF_PWM20);
static bool s_pwm_began = false;
static nrf_pwm_values_individual_t s_pwm_duty;

/* top_value from NRFX_PWM_DEFAULT_CONFIG is 1000 -- duty values are
 * 0..1000 internally; analogWrite()'s 0..255 Arduino-style range is
 * scaled up to that on every call. */
#define PWM_TOP_VALUE 1000

static void pwm_lazy_init(void)
{
    if (s_pwm_began)
    {
        return;
    }

    nrfx_pwm_config_t config = NRFX_PWM_DEFAULT_CONFIG(PIN_PWM0, PIN_PWM1, PIN_PWM2, PIN_PWM3);
    config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;

    s_pwm_duty.channel_0 = 0;
    s_pwm_duty.channel_1 = 0;
    s_pwm_duty.channel_2 = 0;
    s_pwm_duty.channel_3 = 0;

    if (nrfx_pwm_init(&s_pwm, &config, NULL, NULL) == 0)
    {
        s_pwm_began = true;
    }
}

void analogWrite(uint32_t pin, uint8_t value)
{
    pwm_lazy_init();
    if (!s_pwm_began)
    {
        return;
    }

    uint16_t duty = ((uint32_t)value * PWM_TOP_VALUE) / 255;

    if (pin == PIN_PWM0)      { s_pwm_duty.channel_0 = duty; }
    else if (pin == PIN_PWM1) { s_pwm_duty.channel_1 = duty; }
    else if (pin == PIN_PWM2) { s_pwm_duty.channel_2 = duty; }
    else if (pin == PIN_PWM3) { s_pwm_duty.channel_3 = duty; }
    else                      { return; /* not one of the 4 PWM-capable pins */ }

    nrf_pwm_sequence_t seq = {0};
    seq.values.p_individual = &s_pwm_duty;
    seq.length = NRF_PWM_VALUES_LENGTH(s_pwm_duty);
    seq.repeats = 0;
    seq.end_delay = 0;

    /* Loop count 0 would mean "don't play" -- use 1 with the peripheral's
     * own looping mechanism (nrfx_pwm_simple_playback internally plays
     * the same sequence as both seq0 and seq1, see its doc comment, so a
     * playback_count of 1 here still produces continuous output). */
    nrfx_pwm_simple_playback(&s_pwm, &seq, 1, NRFX_PWM_FLAG_LOOP);
}

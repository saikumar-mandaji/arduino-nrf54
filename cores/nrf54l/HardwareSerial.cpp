/**
 * @file HardwareSerial.cpp
 * @brief Serial over UARTE30, blocking TX, polled RX (v1 -- see
 *        docs/VERIFICATION.md for what's not yet implemented, e.g.
 *        interrupt-driven RX buffering).
 *
 * UARTE30 (not UARTE20) because that's the instance actually wired to
 * the nRF54L15-DK's onboard J-Link VCOM bridge -- confirmed against
 * Zephyr's own nrf54l15dk devicetree (uart30, TX=P0.00, RX=P0.01) after
 * the original UARTE20 guess produced zero output on real hardware. See
 * docs/VERIFICATION.md for the full hardware bring-up account.
 */
#include "HardwareSerial.h"
#include "Arduino.h"
#include <nrfx_uarte.h>

static nrfx_uarte_t s_uarte = NRFX_UARTE_INSTANCE(NRF_UARTE30);

/* Single-byte non-blocking RX polling: nrfx_uarte's RX path is buffer-
 * oriented (nrfx_uarte_rx_buffer_set), so v1 keeps one byte in flight at a
 * time and re-arms it after each read() -- simple, but not interrupt-
 * driven, so bytes arriving faster than the sketch calls Serial.read()
 * can be dropped. A ring-buffer RX (matching upstream Arduino cores) is
 * a known follow-up, not implemented here. */
static uint8_t s_rx_byte;
static volatile bool s_rx_byte_ready = false;

static void uarte_event_handler(nrfx_uarte_event_t const * p_event, void * p_context)
{
    (void)p_context;
    if (p_event->type == NRFX_UARTE_EVT_RX_DONE)
    {
        s_rx_byte_ready = true;
    }
    else if (p_event->type == NRFX_UARTE_EVT_TX_DONE)
    {
        /* Nothing to do: writes in v1 use NRFX_UARTE_TX_BLOCKING. */
    }
}

static void rx_rearm(void)
{
    s_rx_byte_ready = false;
    (void)nrfx_uarte_rx_buffer_set(&s_uarte, &s_rx_byte, 1);
}

/* REAL BUG FOUND ON HARDWARE (2026-07-21): nrfx_uarte's IRQ handler is
 * generic and instance-parameterized (nrfx_uarte_irq_handler(nrfx_uarte_t*)),
 * unlike GRTC's fixed no-argument handler -- with NRFX_PRS_ENABLED=0
 * (this core's config), nrfx never wires this to the real vector table
 * itself; the integrator must provide a statically-named trampoline for
 * the preprocessor rename chain in nrfx_irqs_nrf54l15_application.h
 * (#define nrfx_uarte_30_irq_handler SERIAL30_IRQHandler) to have
 * anything to rename. Without this, SERIAL30_IRQHandler stayed weak-
 * aliased to Default_Handler -- confirmed via `nm` on a real build
 * before this fix -- so RX_DONE events (and therefore Serial.read())
 * never actually fired the callback above. See cores/nrf54l/nrfx_config.h
 * for the related GRTC-vector fix found the same way, and
 * docs/VERIFICATION.md. */
extern "C" void nrfx_uarte_30_irq_handler(void)
{
    nrfx_uarte_irq_handler(&s_uarte);
}

HardwareSerial::HardwareSerial() : _began(false)
{
}

void HardwareSerial::begin(uint32_t baudrate)
{
    nrfx_uarte_config_t config = NRFX_UARTE_DEFAULT_CONFIG(PIN_SERIAL_TX, PIN_SERIAL_RX);

    switch (baudrate)
    {
        case 9600:   config.baudrate = NRF_UARTE_BAUDRATE_9600;   break;
        case 19200:  config.baudrate = NRF_UARTE_BAUDRATE_19200;  break;
        case 38400:  config.baudrate = NRF_UARTE_BAUDRATE_38400;  break;
        case 57600:  config.baudrate = NRF_UARTE_BAUDRATE_57600;  break;
        case 115200: config.baudrate = NRF_UARTE_BAUDRATE_115200; break;
        case 230400: config.baudrate = NRF_UARTE_BAUDRATE_230400; break;
        case 460800: config.baudrate = NRF_UARTE_BAUDRATE_460800; break;
        case 921600: config.baudrate = NRF_UARTE_BAUDRATE_921600; break;
        default:     config.baudrate = NRF_UARTE_BAUDRATE_115200; break;
    }

    if (nrfx_uarte_init(&s_uarte, &config, uarte_event_handler) == 0)
    {
        _began = true;
        rx_rearm();
    }
}

void HardwareSerial::end()
{
    if (_began)
    {
        nrfx_uarte_uninit(&s_uarte);
        _began = false;
    }
}

int HardwareSerial::available()
{
    return (_began && s_rx_byte_ready) ? 1 : 0;
}

int HardwareSerial::read()
{
    if (!available())
    {
        return -1;
    }
    int c = s_rx_byte;
    rx_rearm();
    return c;
}

int HardwareSerial::peek()
{
    if (!available())
    {
        return -1;
    }
    return s_rx_byte;
}

size_t HardwareSerial::write(uint8_t c)
{
    return write(&c, 1);
}

size_t HardwareSerial::write(const uint8_t * buffer, size_t size)
{
    if (!_began || size == 0)
    {
        return 0;
    }
    int err = nrfx_uarte_tx(&s_uarte, buffer, size, NRFX_UARTE_TX_BLOCKING);
    return (err == 0) ? size : 0;
}

HardwareSerial Serial;

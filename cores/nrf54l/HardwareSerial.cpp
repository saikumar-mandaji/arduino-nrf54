/**
 * @file HardwareSerial.cpp
 * @brief Serial over UARTE20, blocking TX, polled RX (v1 -- see
 *        docs/VERIFICATION.md for what's not yet implemented, e.g.
 *        interrupt-driven RX buffering).
 *
 * REAL BUG FOUND AND FIXED (2026-07-21), root cause of the long-standing
 * "Serial produces no output visible to automated tooling" mystery: this
 * project previously used UARTE30 (TX=P0.00/RX=P0.01), reasoning that
 * an earlier UARTE20 attempt (with GUESSED pins P1.00/P1.01) had
 * produced no output. Re-reading Zephyr's real nrf54l15dk devicetree
 * directly (not from memory) shows the DK's actual chosen console is
 * `zephyr,console = &uart20`, with uart20's *real* pinctrl being
 * TX=P1.04, RX=P1.05, RTS=P1.06, CTS=P1.07 -- P1.00/P1.01 (the earlier
 * guess) was simply the wrong pins for the right instance. `uart30`
 * (P0.00/P0.01) exists in the devicetree and is configured, but is not
 * the chosen console -- it's a secondary UART, not the one bridged to
 * the DK's automated-tooling-visible VCOM port. Switched to UARTE20
 * with the correct P1.04/P1.05 pins (RTS/CTS not wired up here -- basic
 * 2-wire TX/RX, no hardware flow control).
 */
#include "HardwareSerial.h"
#include "Arduino.h"
#include <nrfx_uarte.h>
#include <string.h>

static nrfx_uarte_t s_uarte = NRFX_UARTE_INSTANCE(NRF_UARTE20);

/* Single-byte non-blocking RX polling: nrfx_uarte's RX path is buffer-
 * oriented (nrfx_uarte_rx_buffer_set), so v1 keeps one byte in flight at a
 * time and re-arms it after each read() -- simple, but not interrupt-
 * driven, so bytes arriving faster than the sketch calls Serial.read()
 * can be dropped. A ring-buffer RX (matching upstream Arduino cores) is
 * a known follow-up, not implemented here. */
static uint8_t s_rx_byte;
static volatile bool s_rx_byte_ready = false;

/* REAL BUG FOUND AND FIXED (2026-07-21), a second, distinct root cause
 * for the Serial mystery affecting RX specifically (TX had its own
 * separate bugs -- see the file header comment and the RAM-buffer fix
 * on write() below). nrfx_uarte_rx_buffer_set() alone does not start
 * reception -- confirmed by reading nrfx_uarte.h's own doc comment for
 * nrfx_uarte_rx_enable() directly: the receiver must be explicitly
 * enabled with nrfx_uarte_rx_enable(), which synchronously fires an
 * NRFX_UARTE_EVT_RX_BUF_REQUEST event that the application must respond
 * to by calling nrfx_uarte_rx_buffer_set() -- and the *same* event
 * fires again every time the current buffer fills, for the app to
 * supply the next one. This core's code previously called
 * nrfx_uarte_rx_buffer_set() once from begin() and again after every
 * read(), but never called nrfx_uarte_rx_enable() at all -- so the
 * receiver was never actually started; PSEL.RXD read back as
 * permanently disconnected (0xFFFFFFFF) on real hardware, confirming
 * this directly. Fixed by calling nrfx_uarte_rx_enable() once in
 * begin() and handling NRFX_UARTE_EVT_RX_BUF_REQUEST here to keep
 * re-supplying the single-byte buffer indefinitely. */
static void uarte_event_handler(nrfx_uarte_event_t const * p_event, void * p_context)
{
    (void)p_context;
    if (p_event->type == NRFX_UARTE_EVT_RX_DONE)
    {
        s_rx_byte_ready = true;
    }
    else if (p_event->type == NRFX_UARTE_EVT_RX_BUF_REQUEST)
    {
        (void)nrfx_uarte_rx_buffer_set(&s_uarte, &s_rx_byte, 1);
    }
    else if (p_event->type == NRFX_UARTE_EVT_TX_DONE)
    {
        /* Nothing to do: writes in v1 use NRFX_UARTE_TX_BLOCKING. */
    }
}

/* REAL BUG FOUND ON HARDWARE (2026-07-21): nrfx_uarte's IRQ handler is
 * generic and instance-parameterized (nrfx_uarte_irq_handler(nrfx_uarte_t*)),
 * unlike GRTC's fixed no-argument handler -- with NRFX_PRS_ENABLED=0
 * (this core's config), nrfx never wires this to the real vector table
 * itself; the integrator must provide a statically-named trampoline for
 * the preprocessor rename chain in nrfx_irqs_nrf54l15_application.h
 * (#define nrfx_uarte_20_irq_handler SERIAL20_IRQHandler) to have
 * anything to rename. Without this, SERIAL20_IRQHandler stayed weak-
 * aliased to Default_Handler -- confirmed via `nm` on a real build
 * before this fix -- so RX_DONE events (and therefore Serial.read())
 * never actually fired the callback above. See cores/nrf54l/nrfx_config.h
 * for the related GRTC-vector fix found the same way, and
 * docs/VERIFICATION.md. */
extern "C" void nrfx_uarte_20_irq_handler(void)
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
        s_rx_byte_ready = false;
        /* rx_enable() synchronously fires NRFX_UARTE_EVT_RX_BUF_REQUEST,
         * which uarte_event_handler() answers with rx_buffer_set() -- see
         * the comment above uarte_event_handler() for why both calls are
         * required to actually start reception. */
        (void)nrfx_uarte_rx_enable(&s_uarte, 0);
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
    /* Re-arming the buffer happens automatically: uarte_event_handler()
     * re-supplies s_rx_byte via rx_buffer_set() every time
     * NRFX_UARTE_EVT_RX_BUF_REQUEST fires (which nrfx raises again once
     * the previous 1-byte buffer fills). Just clear the ready flag here. */
    s_rx_byte_ready = false;
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

/* REAL BUG FOUND AND FIXED (2026-07-21), the actual root cause of the
 * long-standing "Serial produces no output visible to any automated
 * tooling" mystery (docs/VERIFICATION.md) -- present since Serial was
 * first implemented, entirely independent of which UARTE instance/pins
 * were used, which is why switching instances never fixed it.
 *
 * nrfx_uarte's EasyDMA requires the TX source buffer to be in Data RAM
 * -- confirmed directly in extern/nrfx/drivers/src/nrfx_uarte.c's
 * poll_out(), which calls nrf_dma_accessible_check() and returns
 * -EINVAL for any buffer that isn't (the check itself, in
 * extern/nrfx/hal/nrf_common.h, is exactly `(addr & 0xE0000000) ==
 * 0x20000000`, i.e. the standard Cortex-M SRAM range). This function
 * used to hand `buffer` straight to `nrfx_uarte_tx()` unmodified --
 * fine for `Serial.write(uint8_t)` (which always passes the address of
 * a local/stack variable, genuinely in RAM), but broken for
 * `Serial.print("string literal")`/`println(...)`: `Print::write(const
 * char*)` calls `write((const uint8_t*)str, strlen(str))`, and C++
 * virtual dispatch resolves that to *this* function with the ORIGINAL
 * flash-resident string-literal pointer -- `Print`'s own base-class
 * `write(buffer, size)` would have been safe here (it loops calling the
 * single-byte `write(uint8_t)` for each byte, always through a stack
 * copy), but this class's own "bulk transfer" override shadowed that
 * safe default and silently broke it. Every `print`/`println` call with
 * a string-literal argument failed at the very first byte (`poll_out()`
 * returning -EINVAL for every byte), sending nothing at all -- matching
 * "0 bytes received" exactly, on every automated tool, every reset,
 * regardless of which UARTE instance or pins were configured.
 *
 * Fixed by copying through a small RAM staging buffer, chunked, before
 * ever handing a pointer to `nrfx_uarte_tx()` -- this makes the
 * function safe regardless of where the caller's buffer actually lives
 * (flash or RAM), without needing to detect which case applies. */
size_t HardwareSerial::write(const uint8_t * buffer, size_t size)
{
    if (!_began || size == 0)
    {
        return 0;
    }

    static uint8_t s_tx_ram_buf[32];
    size_t written = 0;

    while (written < size)
    {
        size_t chunk = size - written;
        if (chunk > sizeof(s_tx_ram_buf))
        {
            chunk = sizeof(s_tx_ram_buf);
        }
        memcpy(s_tx_ram_buf, buffer + written, chunk);

        int err = nrfx_uarte_tx(&s_uarte, s_tx_ram_buf, chunk, NRFX_UARTE_TX_BLOCKING);
        if (err != 0)
        {
            return written;
        }
        written += chunk;
    }

    return written;
}

HardwareSerial Serial;

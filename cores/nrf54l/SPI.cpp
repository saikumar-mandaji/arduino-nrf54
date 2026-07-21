/**
 * @file SPI.cpp
 * @brief SPI over SPIM21, blocking transfers only.
 *
 * CS/SS is NOT handed to nrfx -- confirmed on real hardware that nrfx_spim
 * auto-asserts/deasserts a configured ss_pin around *every single*
 * nrfx_spim_xfer() call, which breaks any multi-byte transaction built
 * from more than one SPI.transfer() call with CS meant to stay low across
 * all of them (e.g. reading a flash chip's JEDEC ID: command byte + 3
 * response bytes must all happen under one continuous CS-low period, but
 * with a real ss_pin configured, each of the 4 SPI.transfer() calls got
 * its own independent CS pulse, and every byte after the first was
 * received as a fresh, invalid command). Real bug, found by reading back
 * garbage (all-zero) JEDEC ID bytes from the DK's onboard flash chip over
 * SWD and diagnosing from there -- see docs/VERIFICATION.md and
 * examples/HWSelfTest. Fixed by using NRF_SPIM_PIN_NOT_CONNECTED for
 * ss_pin and leaving CS entirely to the sketch via digitalWrite(), which
 * is also the standard Arduino SPI convention on other cores.
 */
#include "SPI.h"
#include "Arduino.h"
#include <nrfx_spim.h>

static nrfx_spim_t s_spim = NRFX_SPIM_INSTANCE(NRF_SPIM21);

/* nrfx_spim's IRQ handler is generic/instance-parameterized, same as
 * nrfx_uarte's (see the trampoline in HardwareSerial.cpp for the full
 * explanation and how this was found on real hardware). Checked
 * nrfx_spim.c directly before adding this: begin() below passes a NULL
 * event handler, and nrfx_spim's own blocking-transfer path (used
 * whenever there's no handler) polls the SPIM END event directly via
 * nrfy_spim_xfer_start()'s blocking mode -- it does NOT wait on this
 * interrupt, so SPI.transfer() was not actually broken by the missing
 * vector. This trampoline is still correct/necessary plumbing, though:
 * any future event-driven (non-blocking) SPI support would need
 * SERIAL21_IRQHandler wired, and it's what nrfx_irqs_nrf54l15_application.h
 * expects the integrator to provide for this instance regardless. */
extern "C" void nrfx_spim_21_irq_handler(void)
{
    nrfx_spim_irq_handler(&s_spim);
}

SPIClass::SPIClass() : _began(false)
{
}

void SPIClass::begin()
{
    nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO, NRF_SPIM_PIN_NOT_CONNECTED);
    if (nrfx_spim_init(&s_spim, &config, NULL, NULL) == 0)
    {
        _began = true;
    }
}

void SPIClass::end()
{
    if (_began)
    {
        nrfx_spim_uninit(&s_spim);
        _began = false;
    }
}

void SPIClass::beginTransaction(SPISettings settings)
{
    if (!_began)
    {
        return;
    }

    nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO, NRF_SPIM_PIN_NOT_CONNECTED);
    config.frequency = settings._clockHz;
    config.bit_order = (settings._bitOrder == LSBFIRST_SPI) ? NRF_SPIM_BIT_ORDER_LSB_FIRST : NRF_SPIM_BIT_ORDER_MSB_FIRST;

    switch (settings._dataMode)
    {
        case SPI_MODE0: config.mode = NRF_SPIM_MODE_0; break;
        case SPI_MODE1: config.mode = NRF_SPIM_MODE_1; break;
        case SPI_MODE2: config.mode = NRF_SPIM_MODE_2; break;
        case SPI_MODE3: config.mode = NRF_SPIM_MODE_3; break;
        default:        config.mode = NRF_SPIM_MODE_0; break;
    }

    nrfx_spim_reconfigure(&s_spim, &config);
}

void SPIClass::endTransaction()
{
    /* No-op: v1 has no per-transaction locking/queuing (single caller,
     * blocking-only transfers) -- see docs/VERIFICATION.md. */
}

uint8_t SPIClass::transfer(uint8_t data)
{
    uint8_t rx = 0xFF;
    nrfx_spim_xfer_desc_t desc = NRFX_SPIM_XFER_TRX(&data, 1, &rx, 1);
    nrfx_spim_xfer(&s_spim, &desc, 0);
    return rx;
}

void SPIClass::transfer(void * buf, size_t count)
{
    /* In-place transfer (standard Arduino SPI.transfer(buf, count)
     * contract): sends buf, overwrites it with the received bytes. Uses
     * the same memory region as both the EasyDMA source and destination
     * pointer -- untested whether SPIM's DMA engine handles that safely
     * for a block transfer versus a genuine byte-at-a-time device; see
     * docs/VERIFICATION.md. */
    transfer(buf, buf, count);
}

void SPIClass::transfer(const void * tx_buf, void * rx_buf, size_t count)
{
    if (!_began || count == 0)
    {
        return;
    }
    nrfx_spim_xfer_desc_t desc = NRFX_SPIM_XFER_TRX(
        (uint8_t const *)tx_buf, tx_buf ? count : 0,
        (uint8_t *)rx_buf, rx_buf ? count : 0);
    nrfx_spim_xfer(&s_spim, &desc, 0);
}

SPIClass SPI;

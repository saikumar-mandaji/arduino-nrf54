/**
 * @file Wire.cpp
 * @brief I2C (TwoWire) over TWIM22, blocking transfers only.
 */
#include "Wire.h"
#include "Arduino.h"
#include <errno.h>
#include <nrfx_twim.h>

static nrfx_twim_t s_twim = NRFX_TWIM_INSTANCE(NRF_TWIM22);

TwoWire::TwoWire() : _began(false), _txLength(0), _txAddress(0), _rxLength(0), _rxIndex(0)
{
}

void TwoWire::begin()
{
    nrfx_twim_config_t config = NRFX_TWIM_DEFAULT_CONFIG(PIN_WIRE_SCL, PIN_WIRE_SDA);
    if (nrfx_twim_init(&s_twim, &config, NULL, NULL) == 0)
    {
        nrfx_twim_enable(&s_twim);
        _began = true;
    }
}

void TwoWire::end()
{
    if (_began)
    {
        nrfx_twim_disable(&s_twim);
        _began = false;
    }
}

void TwoWire::setClock(uint32_t frequencyHz)
{
    if (!_began)
    {
        return;
    }

    nrfx_twim_config_t config = NRFX_TWIM_DEFAULT_CONFIG(PIN_WIRE_SCL, PIN_WIRE_SDA);

    if (frequencyHz >= 1000000)      config.frequency = NRF_TWIM_FREQ_1000K;
    else if (frequencyHz >= 400000)  config.frequency = NRF_TWIM_FREQ_400K;
    else if (frequencyHz >= 250000)  config.frequency = NRF_TWIM_FREQ_250K;
    else                              config.frequency = NRF_TWIM_FREQ_100K;

    nrfx_twim_reconfigure(&s_twim, &config);
}

void TwoWire::beginTransmission(uint8_t address)
{
    _txAddress = address;
    _txLength = 0;
}

uint8_t TwoWire::endTransmission(bool sendStop)
{
    if (!_began)
    {
        return 4; /* "other error", matching the standard Wire error codes */
    }

    nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_TX(_txAddress, _txBuffer, _txLength);
    uint32_t flags = sendStop ? 0 : NRFX_TWIM_FLAG_TX_NO_STOP;
    int err = nrfx_twim_xfer(&s_twim, &desc, flags);

    _txLength = 0;

    switch (err)
    {
        case 0:       return 0; /* success */
        case -EFAULT: return 2; /* NACK on address */
        case -EAGAIN: return 3; /* NACK on data */
        default:      return 4; /* other error */
    }
}

size_t TwoWire::requestFrom(uint8_t address, size_t quantity, bool sendStop)
{
    if (!_began || quantity == 0)
    {
        _rxLength = 0;
        _rxIndex = 0;
        return 0;
    }

    if (quantity > WIRE_BUFFER_LENGTH)
    {
        quantity = WIRE_BUFFER_LENGTH;
    }

    nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_RX(address, _rxBuffer, quantity);
    uint32_t flags = sendStop ? 0 : NRFX_TWIM_FLAG_TX_NO_STOP;
    int err = nrfx_twim_xfer(&s_twim, &desc, flags);

    if (err != 0)
    {
        _rxLength = 0;
        _rxIndex = 0;
        return 0;
    }

    _rxLength = quantity;
    _rxIndex = 0;
    return quantity;
}

size_t TwoWire::write(uint8_t data)
{
    if (_txLength >= WIRE_BUFFER_LENGTH)
    {
        return 0;
    }
    _txBuffer[_txLength++] = data;
    return 1;
}

size_t TwoWire::write(const uint8_t * data, size_t quantity)
{
    size_t n = 0;
    while (n < quantity && _txLength < WIRE_BUFFER_LENGTH)
    {
        _txBuffer[_txLength++] = data[n++];
    }
    return n;
}

int TwoWire::available()
{
    return (int)(_rxLength - _rxIndex);
}

int TwoWire::read()
{
    if (_rxIndex >= _rxLength)
    {
        return -1;
    }
    return _rxBuffer[_rxIndex++];
}

int TwoWire::peek()
{
    if (_rxIndex >= _rxLength)
    {
        return -1;
    }
    return _rxBuffer[_rxIndex];
}

TwoWire Wire;

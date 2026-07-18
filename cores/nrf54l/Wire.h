/**
 * @file Wire.h
 * @brief Minimal Arduino I2C (TwoWire) implementation over TWIM22
 *        (blocking). v1 scope: single fixed instance, master mode only
 *        (no slave mode). See docs/VERIFICATION.md.
 */
#ifndef WIRE_H
#define WIRE_H

#include <stdint.h>
#include <stddef.h>

#define WIRE_BUFFER_LENGTH 32

class TwoWire
{
public:
    TwoWire();

    void begin();
    void end();
    void setClock(uint32_t frequencyHz);

    void beginTransmission(uint8_t address);
    uint8_t endTransmission(bool sendStop = true);

    size_t requestFrom(uint8_t address, size_t quantity, bool sendStop = true);

    size_t write(uint8_t data);
    size_t write(const uint8_t * data, size_t quantity);

    int available();
    int read();
    int peek();

private:
    bool _began;
    uint8_t _txBuffer[WIRE_BUFFER_LENGTH];
    size_t _txLength;
    uint8_t _txAddress;

    uint8_t _rxBuffer[WIRE_BUFFER_LENGTH];
    size_t _rxLength;
    size_t _rxIndex;
};

extern TwoWire Wire;

#endif /* WIRE_H */

/**
 * @file HardwareSerial.h
 * @brief Minimal blocking Serial implementation over UARTE20.
 *
 * v1 scope: blocking write() only (used by print()/println()), no
 * interrupt-driven RX buffering yet -- read()/available() poll the UARTE
 * RX path directly. See docs/VERIFICATION.md.
 */
#ifndef HARDWARE_SERIAL_H
#define HARDWARE_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include "Print.h"

class HardwareSerial : public Print
{
public:
    HardwareSerial();

    void begin(uint32_t baudrate);
    void end();

    int available();
    int read();
    int peek();

    using Print::write;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t * buffer, size_t size) override;

    operator bool() { return true; }

private:
    bool _began;
};

extern HardwareSerial Serial;

#endif /* HARDWARE_SERIAL_H */

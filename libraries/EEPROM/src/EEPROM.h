/**
 * @file EEPROM.h
 * @brief Arduino EEPROM API over a reserved region of this chip's real
 *        RRAM (Resistive RAM).
 *
 * REAL FINDING while implementing this (see docs/VERIFICATION.md): the
 * nRF54L15 has no classic NVMC-style flash peripheral at all -- the
 * generic `nrfx_nvmc` driver in this project's vendored nrfx tree does
 * not even compile for this chip (it references NVMC_CONFIG_WEN_*
 * registers that don't exist on nRF54L15's register map). The real
 * peripheral is RRAMC (Resistive RAM Controller, `nrfx_rramc`), and
 * unlike NOR flash, RRAM is genuinely byte-addressable and
 * byte-*writable* in place -- no page erase is needed before rewriting
 * a byte. That means, unlike ESP8266/ESP32's flash-emulated EEPROM.h
 * (which requires an explicit commit() to erase+rewrite a whole page),
 * write()/update() here persist immediately, matching classic AVR
 * EEPROM.h semantics exactly. commit() is kept only for source
 * compatibility with sketches ported from ESP8266/ESP32 that call it
 * out of habit -- it's a real no-op here, not a stub pretending to do
 * something.
 */
#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

class EEPROMClass
{
public:
    /* size is clamped to the underlying reserved region (see
     * EEPROM.cpp); pass 0 to use the full reserved region. */
    bool begin(size_t size = 0);
    void end();

    uint8_t read(int address);
    void write(int address, uint8_t value);
    void update(int address, uint8_t value);

    template <typename T>
    T &get(int address, T &t)
    {
        if (!_began || address < 0 || (size_t)address + sizeof(T) > _size)
        {
            return t;
        }
        readBytes(address, (uint8_t *)&t, sizeof(T));
        return t;
    }

    template <typename T>
    const T &put(int address, const T &t)
    {
        if (!_began || address < 0 || (size_t)address + sizeof(T) > _size)
        {
            return t;
        }
        writeBytes(address, (const uint8_t *)&t, sizeof(T));
        return t;
    }

    size_t length() const { return _size; }

    /* No-op: writes already persist immediately on this chip's RRAM.
     * Kept only so sketches written against ESP8266/ESP32's EEPROM.h
     * (which require it) still compile and behave correctly here. */
    bool commit() { return true; }

private:
    void readBytes(int address, uint8_t *dst, size_t n);
    void writeBytes(int address, const uint8_t *src, size_t n);

    size_t _size = 0;
    uint32_t _baseAddress = 0;
    bool _began = false;
};

extern EEPROMClass EEPROM;

#endif /* EEPROM_H */

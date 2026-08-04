/**
 * @file EEPROM.cpp
 * @brief See EEPROM.h for why this is a thin, direct wrapper over RRAMC
 *        rather than a flash-erase-emulated cache.
 */
#include "EEPROM.h"
#include <nrfx_rramc.h>
#include <errno.h>

/* Reserves a real region of RRAM via a normally linked `const` array,
 * rather than a hand-picked address -- the linker places this like any
 * other .rodata object, so it can never silently overlap code/data.
 * 4096 bytes is a generous default for calibration-constant-style
 * sensor library usage without meaningfully denting this chip's ~1.5MB
 * of RRAM. */
#define EEPROM_RESERVED_BYTES 4096

__attribute__((aligned(4)))
static const uint8_t s_eepromReservedRegion[EEPROM_RESERVED_BYTES] = {0};

bool EEPROMClass::begin(size_t size)
{
    if (size == 0 || size > EEPROM_RESERVED_BYTES)
    {
        size = EEPROM_RESERVED_BYTES;
    }

    if (!_began)
    {
        nrfx_rramc_config_t config = NRFX_RRAMC_DEFAULT_CONFIG(0);
        config.mode_write = true;
        int err = nrfx_rramc_init(&config, NULL);
        if (err != 0 && err != -EALREADY)
        {
            return false;
        }
        nrfx_rramc_write_enable_set(true, 0);
    }

    _baseAddress = (uint32_t)s_eepromReservedRegion;
    _size = size;
    _began = true;
    return true;
}

void EEPROMClass::end()
{
    _began = false;
    _size = 0;
}

uint8_t EEPROMClass::read(int address)
{
    if (!_began || address < 0 || (size_t)address >= _size)
    {
        return 0;
    }
    return nrfx_rramc_byte_read(_baseAddress + (uint32_t)address);
}

void EEPROMClass::write(int address, uint8_t value)
{
    if (!_began || address < 0 || (size_t)address >= _size)
    {
        return;
    }
    nrfx_rramc_byte_write(_baseAddress + (uint32_t)address, value);
}

void EEPROMClass::update(int address, uint8_t value)
{
    if (read(address) != value)
    {
        write(address, value);
    }
}

void EEPROMClass::readBytes(int address, uint8_t *dst, size_t n)
{
    nrfx_rramc_buffer_read(dst, _baseAddress + (uint32_t)address, (uint32_t)n);
}

void EEPROMClass::writeBytes(int address, const uint8_t *src, size_t n)
{
    nrfx_rramc_bytes_write(_baseAddress + (uint32_t)address, src, (uint32_t)n);
}

EEPROMClass EEPROM;

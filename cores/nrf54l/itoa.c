/**
 * @file itoa.c
 * @brief Portable implementation of the avr-libc itoa()/ltoa()/utoa()/
 *        ultoa() family, required by ArduinoCore-API's String class
 *        (api/itoa.h) on non-AVR targets that don't provide these in
 *        their C library. arm-none-eabi's newlib does not.
 */
#include "itoa.h"

static char *utoa_generic(unsigned long value, char *string, int radix)
{
    char buf[8 * sizeof(unsigned long) + 1];
    char *p = &buf[sizeof(buf) - 1];
    *p = '\0';

    if (radix < 2 || radix > 36)
    {
        radix = 10;
    }

    do
    {
        unsigned long m = value;
        value /= (unsigned long)radix;
        char digit = (char)(m - (unsigned long)radix * value);
        *--p = (digit < 10) ? (char)(digit + '0') : (char)(digit - 10 + 'a');
    } while (value);

    char *out = string;
    while ((*out++ = *p++) != '\0')
        ;
    return string;
}

char *ultoa(unsigned long value, char *string, int radix)
{
    return utoa_generic(value, string, radix);
}

char *utoa(unsigned value, char *string, int radix)
{
    return utoa_generic((unsigned long)value, string, radix);
}

char *ltoa(long value, char *string, int radix)
{
    if (radix == 10 && value < 0)
    {
        string[0] = '-';
        /* Negate via unsigned arithmetic to avoid signed overflow UB on
         * LONG_MIN, which has no positive representation as a long. */
        utoa_generic((0UL - (unsigned long)value), string + 1, radix);
        return string;
    }
    return utoa_generic((unsigned long)value, string, radix);
}

char *itoa(int value, char *string, int radix)
{
    return ltoa((long)value, string, radix);
}

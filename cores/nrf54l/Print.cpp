#include "Print.h"
#include <string.h>

size_t Print::write(const uint8_t * buffer, size_t size)
{
    size_t n = 0;
    while (n < size)
    {
        n += write(buffer[n]);
    }
    return n;
}

size_t Print::write(const char * str)
{
    if (str == nullptr)
    {
        return 0;
    }
    return write((const uint8_t *)str, strlen(str));
}

size_t Print::print(const char * str)
{
    return write(str);
}

size_t Print::print(char c)
{
    return write((uint8_t)c);
}

size_t Print::printNumber(unsigned long n, uint8_t base, bool negative)
{
    char buf[8 * sizeof(long) + 1];
    char * str = &buf[sizeof(buf) - 1];
    *str = '\0';

    if (base < 2)
    {
        base = 10;
    }

    do
    {
        unsigned long m = n;
        n /= base;
        char digit = (char)(m - base * n);
        *--str = (digit < 10) ? (char)(digit + '0') : (char)(digit - 10 + 'A');
    } while (n);

    if (negative)
    {
        *--str = '-';
    }

    return write(str);
}

size_t Print::print(int n, int base)
{
    return print((long)n, base);
}

size_t Print::print(unsigned int n, int base)
{
    return print((unsigned long)n, base);
}

size_t Print::print(long n, int base)
{
    if (base == 10 && n < 0)
    {
        return printNumber((unsigned long)(-n), 10, true);
    }
    return printNumber((unsigned long)n, (uint8_t)base, false);
}

size_t Print::print(unsigned long n, int base)
{
    return printNumber(n, (uint8_t)base, false);
}

size_t Print::println()
{
    return write("\r\n");
}

size_t Print::println(const char * str)
{
    size_t n = print(str);
    n += println();
    return n;
}

size_t Print::println(char c)
{
    size_t n = print(c);
    n += println();
    return n;
}

size_t Print::println(int n, int base)
{
    size_t c = print(n, base);
    c += println();
    return c;
}

size_t Print::println(unsigned int n, int base)
{
    size_t c = print(n, base);
    c += println();
    return c;
}

size_t Print::println(long n, int base)
{
    size_t c = print(n, base);
    c += println();
    return c;
}

size_t Print::println(unsigned long n, int base)
{
    size_t c = print(n, base);
    c += println();
    return c;
}

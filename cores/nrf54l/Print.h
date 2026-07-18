/**
 * @file Print.h
 * @brief Minimal Print base class (subset of the standard Arduino Print
 *        API) providing print()/println() on top of a subclass's write().
 */
#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include <stddef.h>

class Print
{
public:
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t * buffer, size_t size);
    size_t write(const char * str);

    size_t print(const char * str);
    size_t print(char c);
    size_t print(int n, int base = 10);
    size_t print(unsigned int n, int base = 10);
    size_t print(long n, int base = 10);
    size_t print(unsigned long n, int base = 10);

    size_t println();
    size_t println(const char * str);
    size_t println(char c);
    size_t println(int n, int base = 10);
    size_t println(unsigned int n, int base = 10);
    size_t println(long n, int base = 10);
    size_t println(unsigned long n, int base = 10);

private:
    size_t printNumber(unsigned long n, uint8_t base, bool negative);
};

#endif /* PRINT_H */

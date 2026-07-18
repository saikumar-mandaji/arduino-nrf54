/**
 * @file SPI.h
 * @brief Minimal Arduino SPI implementation over SPIM21 (blocking).
 *
 * v1 scope: single fixed instance (SPIM21), begin()/beginTransaction()
 * apply settings and are otherwise equivalent -- no multi-device
 * transaction queuing, no DMA-async transfer. See docs/VERIFICATION.md.
 */
#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stddef.h>

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

#define MSBFIRST_SPI 0
#define LSBFIRST_SPI 1

class SPISettings
{
public:
    SPISettings(uint32_t clockHz, uint8_t bitOrder, uint8_t dataMode)
        : _clockHz(clockHz), _bitOrder(bitOrder), _dataMode(dataMode) {}
    SPISettings() : _clockHz(4000000), _bitOrder(MSBFIRST_SPI), _dataMode(SPI_MODE0) {}

    uint32_t _clockHz;
    uint8_t _bitOrder;
    uint8_t _dataMode;
};

class SPIClass
{
public:
    SPIClass();

    void begin();
    void end();

    void beginTransaction(SPISettings settings);
    void endTransaction();

    uint8_t transfer(uint8_t data);
    void transfer(void * buf, size_t count);
    /* Full-duplex transfer: writes tx_buf, reads into rx_buf, both count
     * bytes long (either may be NULL to skip that direction). */
    void transfer(const void * tx_buf, void * rx_buf, size_t count);

private:
    bool _began;
    SPISettings _pendingSettings;
};

extern SPIClass SPI;

#endif /* SPI_H */

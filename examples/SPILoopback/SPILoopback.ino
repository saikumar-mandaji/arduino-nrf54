/*
  SPILoopback -- arduino-nrf54 v2 acceptance test.

  Requires MOSI physically jumpered to MISO (a standard SPI bring-up
  test -- no specific SPI peripheral chip needed). Sends a known byte
  pattern and confirms it reads back unchanged.
  PIN_SPI_* are BEST-EFFORT PLACEHOLDERS (see
  variants/nrf54l15dk/pins_arduino.h) -- verify against your board, and
  jumper MOSI to MISO on the actual pins used, not the silkscreen labels,
  until those placeholders are confirmed.
*/
#include <Arduino.h>
#include <SPI.h>

void setup()
{
    Serial.begin(115200);
    SPI.begin();
    Serial.println("arduino-nrf54 SPILoopback starting");
}

void loop()
{
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST_SPI, SPI_MODE0));

    bool ok = true;
    for (uint16_t i = 0; i < 256; i++)
    {
        uint8_t sent = (uint8_t)i;
        uint8_t received = SPI.transfer(sent);
        if (received != sent)
        {
            ok = false;
            Serial.print("Mismatch: sent 0x");
            Serial.print(sent, 16);
            Serial.print(" got 0x");
            Serial.println(received, 16);
        }
    }

    SPI.endTransaction();

    Serial.println(ok ? "Loopback OK (256/256 bytes matched)" : "Loopback FAILED");
    delay(2000);
}

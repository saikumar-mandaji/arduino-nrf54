/*
  I2CScanner -- arduino-nrf54 v2 acceptance test.

  Scans all 7-bit I2C addresses (0x08-0x77) and reports which ones ACK.
  Doesn't assume any specific device on the bus -- useful as a real
  bring-up test for confirming Wire actually talks to *something*.
  PIN_WIRE_SDA/PIN_WIRE_SCL are BEST-EFFORT PLACEHOLDERS (see
  variants/nrf54l15dk/pins_arduino.h) -- verify against your board.
*/
#include <Arduino.h>
#include <Wire.h>

void setup()
{
    Serial.begin(115200);
    Wire.begin();
    Serial.println("arduino-nrf54 I2CScanner starting");
}

void loop()
{
    Serial.println("Scanning...");
    int found = 0;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();

        if (err == 0)
        {
            Serial.print("Found device at 0x");
            Serial.println(addr, 16);
            found++;
        }
    }

    if (found == 0)
    {
        Serial.println("No I2C devices found");
    }

    delay(5000);
}

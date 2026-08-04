/*
  EEPROMWriteRead -- exercises the EEPROM library's flash-emulated
  read/write/update/commit cycle.

  Writes an incrementing counter to address 0 on every boot, persisted
  across resets via commit(), and prints it back. See EEPROM.h for why
  commit() is required (this is flash-emulated, not real EEPROM).
*/
#include <Arduino.h>
#include <EEPROM.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("arduino-nrf54 EEPROMWriteRead starting");

    if (!EEPROM.begin())
    {
        Serial.println("EEPROM.begin() failed");
        return;
    }

    uint8_t bootCount = EEPROM.read(0);
    Serial.print("Boot count read from flash: ");
    Serial.println(bootCount);

    bootCount++;
    EEPROM.write(0, bootCount);
    if (EEPROM.commit())
    {
        Serial.println("Committed new boot count to flash");
    }
    else
    {
        Serial.println("EEPROM.commit() failed");
    }
}

void loop()
{
}

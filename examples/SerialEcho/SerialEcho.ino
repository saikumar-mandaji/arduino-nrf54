/*
  SerialEcho -- arduino-nrf54 v1 acceptance test.

  Echoes back everything received on Serial (UARTE20). Confirms both the
  blocking TX path and the polled single-byte RX path work end to end.
  PIN_SERIAL_TX/PIN_SERIAL_RX are BEST-EFFORT PLACEHOLDERS (see
  variants/nrf54l15dk/pins_arduino.h) -- verify against your board before
  expecting this to reach a terminal.
*/
#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("arduino-nrf54 SerialEcho ready");
}

void loop()
{
    if (Serial.available())
    {
        int c = Serial.read();
        Serial.write((uint8_t)c);
    }
}

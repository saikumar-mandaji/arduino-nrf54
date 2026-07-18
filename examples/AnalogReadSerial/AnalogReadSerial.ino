/*
  AnalogReadSerial -- arduino-nrf54 v3 acceptance test.

  Reads A0 (SAADC AIN0) and prints the 12-bit result over Serial.
  A0 is an SAADC channel index, not a GPIO pin number -- see
  variants/nrf54l15dk/pins_arduino.h for the physical-pin caveat.
*/
#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("arduino-nrf54 AnalogReadSerial starting");
}

void loop()
{
    int value = analogRead(A0);
    Serial.print("A0 = ");
    Serial.println(value);
    delay(500);
}

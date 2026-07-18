/*
  Blink -- arduino-nrf54 v1 acceptance test.

  Blinks LED_BUILTIN. LED_BUILTIN's pin/polarity are BEST-EFFORT
  PLACEHOLDERS (see variants/nrf54l15dk/pins_arduino.h) -- verify against
  your nRF54L15-DK's actual schematic/User Guide before assuming this
  lights the right LED the right way.
*/
#include <Arduino.h>

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    Serial.println("arduino-nrf54 Blink starting");
}

void loop()
{
#if LED_BUILTIN_ACTIVE_LOW
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
#else
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
#endif
}

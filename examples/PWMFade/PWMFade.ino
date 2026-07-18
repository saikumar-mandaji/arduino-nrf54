/*
  PWMFade -- arduino-nrf54 v3 acceptance test.

  Fades an LED connected to PIN_PWM0 up and down. PIN_PWM0 is one of the
  4 fixed PWM20-capable pins (see variants/nrf54l15dk/pins_arduino.h) --
  analogWrite() only works on PIN_PWM0..PIN_PWM3, not arbitrary GPIOs.
*/
#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("arduino-nrf54 PWMFade starting");
}

void loop()
{
    for (int duty = 0; duty <= 255; duty++)
    {
        analogWrite(PIN_PWM0, duty);
        delay(5);
    }
    for (int duty = 255; duty >= 0; duty--)
    {
        analogWrite(PIN_PWM0, duty);
        delay(5);
    }
}

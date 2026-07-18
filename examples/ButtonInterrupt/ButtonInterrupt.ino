/*
  ButtonInterrupt -- arduino-nrf54 v4 acceptance test.

  Toggles LED_BUILTIN from a GPIOTE interrupt on BTN1 (falling edge --
  BTN1 is expected active-low with an internal pull-up, standard for a
  DK button). BTN1/LED_BUILTIN pins are BEST-EFFORT PLACEHOLDERS (see
  variants/nrf54l15dk/pins_arduino.h) -- verify against your board.
*/
#include <Arduino.h>

volatile bool ledState = false;

void onButtonPress()
{
    ledState = !ledState;
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BTN1, INPUT_PULLUP);
    attachInterrupt(BTN1, onButtonPress, FALLING);
    Serial.println("arduino-nrf54 ButtonInterrupt starting");
}

void loop()
{
#if LED_BUILTIN_ACTIVE_LOW
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
#else
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
#endif
    delay(10);
}

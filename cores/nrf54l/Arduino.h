/**
 * @file Arduino.h
 * @brief Top-level Arduino API header for arduino-nrf54 (v1: GPIO, Serial,
 *        millis()/micros()/delay() only -- see docs/VERIFICATION.md for
 *        the full scope disclosure).
 */
#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIGH 0x1
#define LOW  0x0

#define INPUT         0x0
#define OUTPUT        0x1
#define INPUT_PULLUP  0x2
#define INPUT_PULLDOWN 0x3

#define LSBFIRST 0
#define MSBFIRST 1

void pinMode(uint32_t pin, uint32_t mode);
void digitalWrite(uint32_t pin, uint32_t value);
int digitalRead(uint32_t pin);

/* pin here is an SAADC channel index (A0..A7), not a GPIO pin -- see
 * variants/nrf54l15dk/pins_arduino.h. Returns a 12-bit value (0-4095). */
int analogRead(uint32_t pin);

/* pin must be one of PIN_PWM0..PIN_PWM3 -- see
 * variants/nrf54l15dk/pins_arduino.h. value is 0-255, matching the
 * standard Arduino analogWrite() range. */
void analogWrite(uint32_t pin, uint8_t value);

#define RISING  0
#define FALLING 1
#define CHANGE  2

typedef void (*isr_callback_t)(void);

/* Up to 8 simultaneous pins (see NRFX_GPIOTE_CONFIG_NUM_OF_EVT_HANDLERS
 * in nrfx_config_nrf54l15_application.h). mode is RISING/FALLING/CHANGE
 * -- level-triggered (LOW/HIGH) interrupts are not supported by this
 * core: GPIOTE channel-based triggering (what attachInterrupt() uses)
 * only supports edge triggers, not level triggers, on the same pin --
 * see docs/ARCHITECTURE.md. */
void attachInterrupt(uint32_t pin, isr_callback_t callback, int mode);
void detachInterrupt(uint32_t pin);

uint32_t millis(void);
uint32_t micros(void);
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);

/* Called once by main() before setup(); not part of the public sketch API. */
void nrf54_core_init(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/* min/max/etc as templates, matching upstream Arduino cores. */
template <typename T> T min_(T a, T b) { return a < b ? a : b; }
template <typename T> T max_(T a, T b) { return a > b ? a : b; }
#ifndef min
#define min(a, b) min_(a, b)
#endif
#ifndef max
#define max(a, b) max_(a, b)
#endif

/* Sketch entry points, implemented by the user's .ino. */
void setup();
void loop();

#include "HardwareSerial.h"
extern HardwareSerial Serial;

#endif /* __cplusplus */

#include "pins_arduino.h"

#endif /* ARDUINO_H */

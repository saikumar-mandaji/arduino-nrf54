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

#include "wiring_wdt.h"
#include "wiring_trng.h"

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

/* Common Arduino API macros/constants many sensor libraries assume are
 * available from Arduino.h, ported from ArduinoCore-API's Common.h
 * (kept as plain macros here rather than pulling in Common.h itself,
 * since its PinStatus/PinMode enums would collide with the HIGH/LOW/
 * INPUT/OUTPUT macros already defined above). */
#define PI          3.1415926535897932384626433832795
#define HALF_PI     1.5707963267948966192313216916398
#define TWO_PI      6.283185307179586476925286766559
#define DEG_TO_RAD  0.017453292519943295769236907684886
#define RAD_TO_DEG  57.295779513082320876798154814105
#define EULER       2.718281828459045235360287471352

#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif
#ifndef radians
#define radians(deg) ((deg)*DEG_TO_RAD)
#endif
#ifndef degrees
#define degrees(rad) ((rad)*RAD_TO_DEG)
#endif
#ifndef sq
#define sq(x) ((x)*(x))
#endif

#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet((value), (bit)) : bitClear((value), (bit)))
#ifndef bit
#define bit(b) (1UL << (b))
#endif

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

typedef bool     boolean;
typedef uint8_t  byte;
typedef uint16_t word;

/* min/max as two-type templates (not single-type), matching upstream
 * Arduino cores -- callers routinely mix types (e.g. min(uint8_t,
 * size_t), as ArduinoCore-API's own WString.cpp does), which a
 * single-type template rejects with a deduction error. */
template <typename T1, typename T2> auto min_(T1 a, T2 b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template <typename T1, typename T2> auto max_(T1 a, T2 b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
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

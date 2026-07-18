/**
 * @file pins_arduino.h
 * @brief Pin mapping for the Nordic nRF54L15-DK (PCA10156).
 *
 * The nRF54L15 has three GPIO ports: P0 (pins 0-6, 7 pins), P1 (pins
 * 0-16, 17 pins), P2 (pins 0-10, 11 pins) -- 35 GPIOs total (verified
 * against extern/nrfx/bsp/stable/mdk/nrf54l/nrf54l15/nrf54l15_application_peripherals.h,
 * P0_PIN_NUM_MAX/P1_PIN_NUM_MAX/P2_PIN_NUM_MAX).
 *
 * Arduino digital pin numbers below are a simple linear mapping across
 * all three ports (d0..d6 = P0.0-P0.6, d7..d23 = P1.0-P1.16,
 * d24..d34 = P2.0-P2.10), NOT the nRF54L15-DK silkscreen numbering.
 *
 * LED_BUILTIN / BTN1 below are BEST-EFFORT PLACEHOLDERS. This project was
 * authored without a physical nRF54L15-DK or its schematic/Hardware User
 * Guide in front of the toolchain -- verify (and correct, if needed)
 * against your board's actual User Guide / schematic during the internal
 * hardware bring-up pass before relying on them. See docs/VERIFICATION.md.
 */
#ifndef PINS_ARDUINO_H
#define PINS_ARDUINO_H

#include <hal/nrf_gpio.h>

#define P0_PIN(n) NRF_GPIO_PIN_MAP(0, n)
#define P1_PIN(n) NRF_GPIO_PIN_MAP(1, n)
#define P2_PIN(n) NRF_GPIO_PIN_MAP(2, n)

/* d0..d6 */
#define D0  P0_PIN(0)
#define D1  P0_PIN(1)
#define D2  P0_PIN(2)
#define D3  P0_PIN(3)
#define D4  P0_PIN(4)
#define D5  P0_PIN(5)
#define D6  P0_PIN(6)

/* d7..d23 */
#define D7  P1_PIN(0)
#define D8  P1_PIN(1)
#define D9  P1_PIN(2)
#define D10 P1_PIN(3)
#define D11 P1_PIN(4)
#define D12 P1_PIN(5)
#define D13 P1_PIN(6)
#define D14 P1_PIN(7)
#define D15 P1_PIN(8)
#define D16 P1_PIN(9)
#define D17 P1_PIN(10)
#define D18 P1_PIN(11)
#define D19 P1_PIN(12)
#define D20 P1_PIN(13)
#define D21 P1_PIN(14)
#define D22 P1_PIN(15)
#define D23 P1_PIN(16)

/* d24..d34 */
#define D24 P2_PIN(0)
#define D25 P2_PIN(1)
#define D26 P2_PIN(2)
#define D27 P2_PIN(3)
#define D28 P2_PIN(4)
#define D29 P2_PIN(5)
#define D30 P2_PIN(6)
#define D31 P2_PIN(7)
#define D32 P2_PIN(8)
#define D33 P2_PIN(9)
#define D34 P2_PIN(10)

/* CONFIRMED (not a placeholder) -- cross-checked against Zephyr's own
 * nrf54l15dk_common.dtsi: led0 = gpio2 pin 9, GPIO_ACTIVE_HIGH. The
 * original P1.09/active-low guess here was wrong on both the pin and
 * the polarity -- see docs/VERIFICATION.md for the bring-up account. */
#define LED_BUILTIN P2_PIN(9)
#define LED_BUILTIN_ACTIVE_LOW 0

/* CONFIRMED (not a placeholder) -- Zephyr's nrf54l15dk_common.dtsi:
 * button0/sw0 = gpio1 pin 13, pull-up + GPIO_ACTIVE_LOW. The original
 * P1.02 guess was wrong. */
#define BTN1 P1_PIN(13)

/* SPI pins, used by SPI.begin() -- CONFIRMED (not a placeholder): these
 * are the DK's onboard MX25R6435F SPI NOR flash chip's real SPI00
 * wiring (SCK=P2.01, MOSI=P2.02, MISO=P2.04, CS=P2.05, JEDEC ID
 * C2 28 17), cross-checked against Zephyr's nrf54l15dk devicetree. SPI
 * is built on SPIM21 (a distinct shared "flexible serial peripheral"
 * block from Serial's UARTE30 -- see docs/ARCHITECTURE.md), but nRF54
 * peripheral pins are fully remappable, so SPIM21 can drive the same
 * physical pins the schematic wires to the onboard SPI00 instance --
 * confirmed working: see examples/HWSelfTest and docs/VERIFICATION.md. */
#define PIN_SPI_SCK  P2_PIN(1)
#define PIN_SPI_MOSI P2_PIN(2)
#define PIN_SPI_MISO P2_PIN(4)
#define PIN_SPI_SS   P2_PIN(5)

/* I2C (Wire) pins -- BEST-EFFORT PLACEHOLDER, see file header. Wire is
 * built on TWIM22 (distinct shared-block index from Serial/SPI). */
#define PIN_WIRE_SDA D21 /* P1.14 -- VERIFY */
#define PIN_WIRE_SCL D22 /* P1.15 -- VERIFY */

/* Analog inputs, used by analogRead() -- these are SAADC channel indices
 * (NRF_SAADC_INPUT_AIN0..AIN7), NOT Arduino digital-pin-numbering-style
 * GPIO pins, and NOT run through NRF_GPIO_PIN_MAP. Which physical GPIO
 * pin each AINn corresponds to is fixed in silicon (see the nRF54L15
 * Product Specification's pin assignment table) -- BEST-EFFORT
 * PLACEHOLDER labels below, VERIFY the physical pin before wiring an
 * analog source to it. */
#define A0 0 /* AIN0 -- VERIFY physical pin */
#define A1 1 /* AIN1 -- VERIFY physical pin */
#define A2 2 /* AIN2 -- VERIFY physical pin */
#define A3 3 /* AIN3 -- VERIFY physical pin */
#define A4 4 /* AIN4 -- VERIFY physical pin */
#define A5 5 /* AIN5 -- VERIFY physical pin */
#define A6 6 /* AIN6 -- VERIFY physical pin */
#define A7 7 /* AIN7 -- VERIFY physical pin */

/* PWM outputs, used by analogWrite() -- fixed set of 4 pins on PWM20
 * (independent hardware from Serial/SPI/Wire's shared-block indices, see
 * docs/ARCHITECTURE.md). analogWrite() only works on these 4 specific
 * pins, not arbitrary GPIOs -- a real hardware limitation of driving PWM
 * through a fixed-channel-count peripheral, not an oversight.
 * BEST-EFFORT PLACEHOLDER, see file header. */
#define PIN_PWM0 D23 /* P1.16 -- VERIFY */
#define PIN_PWM1 D24 /* P2.00 -- VERIFY */
#define PIN_PWM2 D25 /* P2.01 -- VERIFY */
#define PIN_PWM3 D26 /* P2.02 -- VERIFY */

/* UARTE30 pins used by HardwareSerial (Serial), routed to the DK's
 * onboard J-Link VCOM UART bridge (VCOM0). CONFIRMED, not a placeholder
 * -- matches Zephyr's own nrf54l15dk_nrf54l15 devicetree (uart30,
 * TX=P0.00, RX=P0.01) and was cross-checked against real hardware after
 * the original UARTE20/P1.00-P1.01 guess produced no serial output at
 * all. See docs/VERIFICATION.md for the bring-up account. */
#define PIN_SERIAL_TX P0_PIN(0)
#define PIN_SERIAL_RX P0_PIN(1)

#endif /* PINS_ARDUINO_H */

/**
 * @file pins_arduino.h
 * @brief Pin mapping for the Ezurio BL54L15 DVK (nRF54L15 module dev kit).
 *
 * Pin data below is derived from Zephyr's own devicetree source for this
 * board (zephyrproject-rtos/zephyr, boards/ezurio/bl54l15_dvk/), the same
 * technique already used successfully for the nRF54L15-DK variant. NOT
 * yet verified against real hardware by this project -- see
 * docs/VERIFICATION.md.
 *
 * This board's onboard SPI NOR flash (MX25R6435F, same JEDEC ID
 * `C2 28 17` this project already confirmed on the nRF54L15-DK) sits on
 * the identical SCK=P2.01/MOSI=P2.02/MISO=P2.04/CS=P2.05 pins as the DK,
 * and its console UART is on the same P0.00 (TX)/P0.01 (RX) pins too --
 * both match Nordic's own reference schematic exactly (cross-checked
 * against the DK's already-hardware-confirmed pins_arduino.h), which is
 * reassuring corroboration even though this specific board hasn't been
 * tested.
 */
#ifndef PINS_ARDUINO_H
#define PINS_ARDUINO_H

#include <hal/nrf_gpio.h>

#define P0_PIN(n) NRF_GPIO_PIN_MAP(0, n)
#define P1_PIN(n) NRF_GPIO_PIN_MAP(1, n)
#define P2_PIN(n) NRF_GPIO_PIN_MAP(2, n)

/* Same linear D0..D34 scheme as the DK variant (same chip, same 3 GPIO
 * ports: P0 0-6, P1 0-16, P2 0-10). This board has no fixed header
 * layout like XIAO's -- it exposes GPIO via a mikroBUS socket and a
 * Qwiic/STEMMA-QT connector, both noted below where they overlap named
 * pins; D-numbers otherwise just follow silicon port/pin order. */
#define D0  P0_PIN(0)
#define D1  P0_PIN(1)
#define D2  P0_PIN(2)
#define D3  P0_PIN(3)
#define D4  P0_PIN(4)
#define D5  P0_PIN(5)
#define D6  P0_PIN(6)

#define D7  P1_PIN(0)
#define D8  P1_PIN(1)
#define D9  P1_PIN(2)
#define D10 P1_PIN(3)
#define D11 P1_PIN(4)
#define D12 P1_PIN(5)
#define D13 P1_PIN(6)
#define D14 P1_PIN(7)
#define D15 P1_PIN(8)  /* also mikroBUS UART RX -- see below */
#define D16 P1_PIN(9)  /* also mikroBUS UART TX -- see below */
#define D17 P1_PIN(10) /* also mikroBUS INT / PWM20 channel 0 -- see below */
#define D18 P1_PIN(11) /* also mikroBUS SCL / Qwiic SCL -- see below */
#define D19 P1_PIN(12) /* also mikroBUS SDA / Qwiic SDA -- see below */
#define D20 P1_PIN(13) /* also mikroBUS AN / button0 -- see below */
#define D21 P1_PIN(14) /* also mikroBUS PWM / LED3 -- see below */
#define D22 P1_PIN(15)
#define D23 P1_PIN(16)

#define D24 P2_PIN(0)
#define D25 P2_PIN(1)  /* also SPI (SPIM21) SCK -- see below */
#define D26 P2_PIN(2)  /* also SPI (SPIM21) MOSI -- see below */
#define D27 P2_PIN(3)
#define D28 P2_PIN(4)  /* also SPI (SPIM21) MISO -- see below */
#define D29 P2_PIN(5)  /* also onboard SPI flash CS -- see below */
#define D30 P2_PIN(6)
#define D31 P2_PIN(7)  /* also LED2 -- see below */
#define D32 P2_PIN(8)
#define D33 P2_PIN(9)  /* also LED0 -- see below */
#define D34 P2_PIN(10)

/* CONFIRMED against bl54l15_dvk_common.dtsi: led0 = gpio2 pin 9,
 * GPIO_ACTIVE_HIGH (matches the DK's LED_BUILTIN pin exactly, though the
 * DK's is active-high too -- same reference design). Three more LEDs
 * (led1=P1.10, led2=P2.07, led3=P1.14) exist on this board but aren't
 * wired to a second Arduino-style constant by this core yet. */
#define LED_BUILTIN P2_PIN(9)
#define LED_BUILTIN_ACTIVE_LOW 0

/* CONFIRMED against bl54l15_dvk_common.dtsi: button0 = gpio1 pin 13,
 * pull-up + GPIO_ACTIVE_LOW (matches the DK's BTN1 pin exactly). Three
 * more buttons exist (button1=P1.09, button2=P1.08, button3=P0.04) but
 * aren't wired to a second Arduino-style constant by this core yet. */
#define BTN1 P1_PIN(13)

/* SPI pins -- CONFIRMED against this board's mikroBUS gpio-map
 * (SCK=P2.01, MOSI=P2.02, MISO=P2.04, CS shared with LED0=P2.09) and the
 * onboard SPI NOR flash's own devicetree node in a sibling board file
 * using the identical P2.01/02/04 pins with CS=P2.05 -- both agree with
 * the DK's already-hardware-confirmed SPI00 pins exactly. Built on
 * SPIM21, same as every other board variant in this project (see
 * docs/ARCHITECTURE.md on remappable peripheral pins). */
#define PIN_SPI_SCK  P2_PIN(1)
#define PIN_SPI_MOSI P2_PIN(2)
#define PIN_SPI_MISO P2_PIN(4)
#define PIN_SPI_SS   P2_PIN(5)

/* I2C (Wire) pins -- CONFIRMED against this board's pinctrl (i2c22_default:
 * SCL=P1.11, SDA=P1.12), which also matches the mikroBUS/Qwiic connector's
 * gpio-map exactly. Built on TWIM22, same as the DK variant. */
#define PIN_WIRE_SDA P1_PIN(12)
#define PIN_WIRE_SCL P1_PIN(11)

/* Analog inputs, used by analogRead() -- same caveat as every other
 * variant in this project: SAADC channel indices, not GPIO pins,
 * BEST-EFFORT PLACEHOLDER. See docs/VERIFICATION.md. */
#define A0 0 /* AIN0 -- VERIFY physical pin */
#define A1 1 /* AIN1 -- VERIFY physical pin */
#define A2 2 /* AIN2 -- VERIFY physical pin */
#define A3 3 /* AIN3 -- VERIFY physical pin */
#define A4 4 /* AIN4 -- VERIFY physical pin */
#define A5 5 /* AIN5 -- VERIFY physical pin */
#define A6 6 /* AIN6 -- VERIFY physical pin */
#define A7 7 /* AIN7 -- VERIFY physical pin */

/* PWM outputs, used by analogWrite() -- only channel 0's pin is
 * CONFIRMED (PWM20 channel 0 = P1.10, from this board's pwm20_default
 * pinctrl, used to drive LED1). The other 3 PWM20 channel pins are
 * BEST-EFFORT PLACEHOLDERS -- VERIFY before relying on them. */
#define PIN_PWM0 P1_PIN(10) /* CONFIRMED: PWM20 channel 0 */
#define PIN_PWM1 D0         /* placeholder -- VERIFY */
#define PIN_PWM2 D1         /* placeholder -- VERIFY */
#define PIN_PWM3 D2         /* placeholder -- VERIFY */

/* Serial (HardwareSerial) pins -- STALE, NEEDS RE-VERIFICATION
 * (2026-07-21): these were matched against this board's uart30 pinctrl
 * on the (now superseded) assumption that HardwareSerial.cpp built on
 * UARTE30. That instance was wrong even for the nRF54L15-DK itself --
 * the core now uses UARTE20 globally (see docs/ARCHITECTURE.md and
 * docs/VERIFICATION.md's "Serial mystery: RESOLVED"). This board has no
 * physical unit in hand to re-verify against, so these pins are now a
 * best-effort placeholder again, not confirmed -- check this board's
 * real uart20 pinctrl node before trusting them. */
#define PIN_SERIAL_TX P0_PIN(0)
#define PIN_SERIAL_RX P0_PIN(1)

#endif /* PINS_ARDUINO_H */

/**
 * @file pins_arduino.h
 * @brief Pin mapping for the Seeed Studio XIAO nRF54L15 / XIAO nRF54L15 Sense.
 *
 * Pin data below is derived from Zephyr's own devicetree source for this
 * board (zephyrproject-rtos/zephyr, boards/seeed/xiao_nrf54l15/), the same
 * technique already used successfully for the nRF54L15-DK variant --
 * cross-checked against the board's `seeed_xiao_connector.dtsi` (the
 * physical 16-pin XIAO header gpio-map) and
 * `xiao_nrf54l15-pinctrl.dtsi` (which peripheral instance/pins the
 * board's UART/I2C/SPI default pin groups use). NOT yet verified against
 * real hardware by this project -- see docs/VERIFICATION.md.
 *
 * Same chip as the DK (nRF54L15, 3 GPIO ports: P0 0-6, P1 0-16, P2 0-10),
 * so the same D0..D34 linear-port-mapping scheme is used for consistency
 * across this project's board variants; D0..D15 below additionally match
 * the XIAO header's own physical D0..D15 silkscreen numbering (confirmed:
 * the gpio-map in `seeed_xiao_connector.dtsi` assigns D0..D15 in that same
 * order to real GPIO pins), so D-numbers on this board are dual-purpose
 * (XIAO header position AND this project's linear scheme happen to agree
 * for D0..D15).
 */
#ifndef PINS_ARDUINO_H
#define PINS_ARDUINO_H

#include <hal/nrf_gpio.h>

#define P0_PIN(n) NRF_GPIO_PIN_MAP(0, n)
#define P1_PIN(n) NRF_GPIO_PIN_MAP(1, n)
#define P2_PIN(n) NRF_GPIO_PIN_MAP(2, n)

/* CONFIRMED against seeed_xiao_connector.dtsi's gpio-map (the XIAO
 * 16-pin header, D0..D15 in silkscreen order). */
#define D0  P1_PIN(4)
#define D1  P1_PIN(5)
#define D2  P1_PIN(6)
#define D3  P1_PIN(7)
#define D4  P1_PIN(10) /* also Wire (TWIM22) SDA -- see below */
#define D5  P1_PIN(11) /* also Wire (TWIM22) SCL -- see below */
#define D6  P2_PIN(8)  /* also Serial (UARTE30) TX -- see below */
#define D7  P2_PIN(7)  /* also Serial (UARTE30) RX -- see below */
#define D8  P2_PIN(1)  /* also SPI (SPIM21) SCK -- see below */
#define D9  P2_PIN(4)  /* also SPI (SPIM21) MISO -- see below */
#define D10 P2_PIN(2)  /* also SPI (SPIM21) MOSI -- see below */
#define D11 P0_PIN(3)  /* secondary I2C (onboard Sense sensors' Wire1), not header-exposed on the base XIAO */
#define D12 P0_PIN(4)  /* secondary I2C (onboard Sense sensors' Wire1), not header-exposed on the base XIAO */
#define D13 P2_PIN(10)
#define D14 P2_PIN(9)
#define D15 P2_PIN(6)

/* d16..d34: remaining GPIO pins on this chip not brought out to the XIAO
 * header, kept for consistency with this project's linear D-numbering
 * scheme on other boards (not physically accessible on this board). */
#define D16 P0_PIN(0)
#define D17 P0_PIN(1)
#define D18 P0_PIN(2)
#define D19 P0_PIN(5)
#define D20 P0_PIN(6)
#define D21 P1_PIN(0)
#define D22 P1_PIN(1)
#define D23 P1_PIN(2)
#define D24 P1_PIN(3)
#define D25 P1_PIN(8)
#define D26 P1_PIN(9)
#define D27 P1_PIN(12)
#define D28 P1_PIN(13)
#define D29 P1_PIN(14)
#define D30 P1_PIN(15)
#define D31 P1_PIN(16)
#define D32 P2_PIN(0)
#define D33 P2_PIN(3)
#define D34 P2_PIN(5)

/* CONFIRMED against xiao_nrf54l15_common.dtsi: led0 = gpio2 pin 0,
 * GPIO_ACTIVE_LOW. */
#define LED_BUILTIN P2_PIN(0)
#define LED_BUILTIN_ACTIVE_LOW 1

/* CONFIRMED against xiao_nrf54l15_common.dtsi: usr_btn (the onboard USR
 * button, not header-exposed) = gpio0 pin 0, pull-up + GPIO_ACTIVE_LOW. */
#define BTN1 P0_PIN(0)

/* SPI pins -- CONFIRMED against xiao_nrf54l15-pinctrl.dtsi's spi00_default
 * group (SCK=P2.01, MOSI=P2.02, MISO=P2.04) and seeed_xiao_connector.dtsi
 * (`xiao_spi: &spi00`), i.e. the standard XIAO D8=SCK/D9=MISO/D10=MOSI
 * convention. This project always drives these physical pins via SPIM21
 * (see docs/ARCHITECTURE.md on remappable peripheral pins), matching how
 * the nRF54L15-DK variant reuses the same physical SPI00-labelled pins.
 * No CS pin is defined by the board itself (SPI CS is a plain digital
 * pin the sketch drives) -- PIN_SPI_SS below is a placeholder, not from
 * devicetree; pick any free D-pin in your sketch. */
#define PIN_SPI_SCK  P2_PIN(1)
#define PIN_SPI_MOSI P2_PIN(2)
#define PIN_SPI_MISO P2_PIN(4)
#define PIN_SPI_SS   D3 /* placeholder -- no fixed CS pin on this board, pick any free pin */

/* I2C (Wire) pins -- CONFIRMED against xiao_nrf54l15-pinctrl.dtsi's
 * i2c22_default group and seeed_xiao_connector.dtsi (`xiao_i2c: &i2c22`),
 * matching the standard XIAO D4=SDA/D5=SCL convention. Built on TWIM22,
 * same as the DK variant. */
#define PIN_WIRE_SDA D4 /* P1.10 */
#define PIN_WIRE_SCL D5 /* P1.11 */

/* Secondary I2C bus (onboard Sense variant IMU/microphone, i2c30 in the
 * devicetree) -- CONFIRMED pins (SDA=P0.04, SCL=P0.03) but NOT wired up
 * as a second Wire instance by this core yet (this core only exposes one
 * Wire/TWIM22 instance today -- see docs/ARCHITECTURE.md). Recorded here
 * for when/if a Wire1 is added; only relevant on the Sense variant. */
#define PIN_WIRE1_SDA D12 /* P0.04 */
#define PIN_WIRE1_SCL D11 /* P0.03 */

/* Analog inputs, used by analogRead() -- SAADC channel indices, same
 * caveat as the DK variant: these are NOT run through NRF_GPIO_PIN_MAP,
 * and which physical pin each AINn corresponds to is fixed in silicon
 * (same across every nRF54L15 board) but not independently re-verified
 * here -- BEST-EFFORT PLACEHOLDER, see docs/VERIFICATION.md. */
#define A0 0 /* AIN0 -- VERIFY physical pin */
#define A1 1 /* AIN1 -- VERIFY physical pin */
#define A2 2 /* AIN2 -- VERIFY physical pin */
#define A3 3 /* AIN3 -- VERIFY physical pin */
#define A4 4 /* AIN4 -- VERIFY physical pin */
#define A5 5 /* AIN5 -- VERIFY physical pin */
#define A6 6 /* AIN6 -- VERIFY physical pin */
#define A7 7 /* AIN7 -- VERIFY physical pin */

/* PWM outputs, used by analogWrite() -- BEST-EFFORT PLACEHOLDER. Zephyr's
 * devicetree for this board only confirms one PWM20 channel used for an
 * onboard LED-adjacent function; it does not confirm all 4 PWM20 channel
 * pins for general use. VERIFY before relying on these. */
#define PIN_PWM0 D0
#define PIN_PWM1 D1
#define PIN_PWM2 D2
#define PIN_PWM3 D3

/* Serial (HardwareSerial) pins -- CONFIRMED against
 * xiao_nrf54l15-pinctrl.dtsi's uart21_default group (TX=P2.08, RX=P2.07)
 * and seeed_xiao_connector.dtsi (`xiao_serial: &uart21`), matching the
 * standard XIAO D6=TX/D7=RX convention. NOTE: this project's
 * HardwareSerial is built on the fixed UARTE20 instance (see
 * docs/ARCHITECTURE.md), so -- exactly like the SPI pins above -- these
 * physical pins are simply routed to UARTE20 rather than the UART21
 * peripheral Zephyr's own driver would use on this board -- nRF's pins
 * are fully remappable, so this is fine regardless of instance name.
 * XIAO nRF54L15 has no onboard USB-serial bridge (unlike the DK's
 * J-Link VCOM), so Serial here means the D6/D7 header pins directly,
 * not a USB port. */
#define PIN_SERIAL_TX D6 /* P2.08 */
#define PIN_SERIAL_RX D7 /* P2.07 */

#endif /* PINS_ARDUINO_H */

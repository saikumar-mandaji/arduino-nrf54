/**
 * @file dtostrf.c
 * @brief dtostrf() implementation for this core, required by
 *        ArduinoCore-API's String class (api/deprecated-avr-comp/avr/
 *        dtostrf.h) on non-AVR targets. Uses ArduinoCore-API's own
 *        upstream reference implementation (LGPL-2.1-or-later, see
 *        third_party_notices/), which relies on newlib's sprintf() float
 *        support -- confirmed available (dtostrf.c.impl's ".global
 *        _printf_float" forces it to link on newlib-nano builds that
 *        otherwise strip float printf support).
 */
#include "deprecated-avr-comp/avr/dtostrf.h"
#include "deprecated-avr-comp/avr/dtostrf.c.impl"

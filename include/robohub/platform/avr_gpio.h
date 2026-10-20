/*
 * avr_gpio.h — GPIO interface implementation for AVR parts.
 *
 * An AVR has three ports of eight pins each, and the pin a program names is the
 * one the board's silkscreen says: 0 to 7 are port B, 8 to 13 are port C and 14
 * to 19 are port D. This maps a pin to the port and the bit behind it, so a
 * driver is written in the pin numbers a person reads off a board.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_GPIO_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/gpio.h"

/* The number of pins an AVR has. */
#define AVR_GPIO_PINS 20u

/* One AVR GPIO line: the interface plus the pin it is on. */
typedef struct {
    RoboHubGPIO interface; /* Must remain the first member. */
    rh_u32      pin;
} RoboHubAVRGPIO;

/* Bind a GPIO line to a pin. */
RoboHubError avr_gpio_create(RoboHubAVRGPIO* gpio, rh_name name, rh_u32 pin);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_GPIO_H */

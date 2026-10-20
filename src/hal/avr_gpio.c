/*
 * avr_gpio.c — GPIO interface implementation for AVR parts.
 *
 * The part's ports are the interface: a pin is a port and a bit, and the two
 * are found from the pin's number. Everything a driver asks for — a direction, a
 * level — is one register write away.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_gpio.h"

#include <avr/io.h>

#include <stddef.h>

/* The port a pin is on, and the bit within it. Pins 0 to 7 are port B, 8 to 13
 * are port C and 14 to 19 are port D. */
static volatile uint8_t* avr_gpio_port(uint32_t pin)
{
    if (pin < 8u) {
        return &PORTB;
    }
    if (pin < 14u) {
        return &PORTC;
    }
    return &PORTD;
}

static volatile uint8_t* avr_gpio_ddr(uint32_t pin)
{
    if (pin < 8u) {
        return &DDRB;
    }
    if (pin < 14u) {
        return &DDRC;
    }
    return &DDRD;
}

static volatile uint8_t* avr_gpio_pin(uint32_t pin)
{
    if (pin < 8u) {
        return &PINB;
    }
    if (pin < 14u) {
        return &PINC;
    }
    return &PIND;
}

static uint8_t avr_gpio_bit(uint32_t pin)
{
    if (pin < 8u) {
        return (uint8_t)(1u << pin);
    }
    if (pin < 14u) {
        return (uint8_t)(1u << (pin - 8u));
    }
    return (uint8_t)(1u << (pin - 14u));
}

static RoboHubError avr_gpio_init(RoboHubGPIO* self)
{
    (void)self;
    return ROBOHUB_OK;
}

static RoboHubError avr_gpio_set_mode(RoboHubGPIO* self, rh_u32 pin, rh_u32 mode)
{
    (void)self;
    if (pin >= AVR_GPIO_PINS) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    uint8_t bit = avr_gpio_bit(pin);
    volatile uint8_t* ddr = avr_gpio_ddr(pin);

    switch (mode) {
    case ROBOHUB_GPIO_MODE_OUTPUT:
        *ddr |= bit;
        break;
    case ROBOHUB_GPIO_MODE_INPUT:
        *ddr &= (uint8_t)~bit;
        break;
    case ROBOHUB_GPIO_MODE_INPUT_PULLUP:
        *ddr &= (uint8_t)~bit;
        *avr_gpio_port(pin) |= bit;
        break;
    default:
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }
    return ROBOHUB_OK;
}

static RoboHubError avr_gpio_write(RoboHubGPIO* self, rh_u32 pin, rh_u32 value)
{
    (void)self;
    if (pin >= AVR_GPIO_PINS) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    uint8_t bit = avr_gpio_bit(pin);
    if (value != 0u) {
        *avr_gpio_port(pin) |= bit;
    } else {
        *avr_gpio_port(pin) &= (uint8_t)~bit;
    }
    return ROBOHUB_OK;
}

static RoboHubError avr_gpio_read(RoboHubGPIO* self, rh_u32 pin, rh_u32* value)
{
    (void)self;
    if (pin >= AVR_GPIO_PINS || value == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    *value = (*avr_gpio_pin(pin) & avr_gpio_bit(pin)) != 0u ? 1u : 0u;
    return ROBOHUB_OK;
}

RoboHubError avr_gpio_create(RoboHubAVRGPIO* gpio, rh_name name, rh_u32 pin)
{
    if (gpio == NULL || pin >= AVR_GPIO_PINS) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    gpio->pin = pin;
    gpio->interface.name = name;
    gpio->interface.context = gpio;
    gpio->interface.init = avr_gpio_init;
    gpio->interface.set_mode = avr_gpio_set_mode;
    gpio->interface.write = avr_gpio_write;
    gpio->interface.read = avr_gpio_read;
    return ROBOHUB_OK;
}

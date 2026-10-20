/*
 * avr_onewire.c — 1-Wire interface implementation for AVR parts.
 *
 * The bus is open-drain: a low is the line driven, a high is the line released
 * with the pull-up holding it, and every pulse is measured against the timer.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_onewire.h"

#include <avr/io.h>

#include <stddef.h>

/* The pulse lengths of the protocol, in microseconds. */
#define ONEWIRE_RESET_LOW_US    480u
#define ONEWIRE_PRESENCE_US      70u
#define ONEWIRE_PRESENCE_WINDOW 300u
#define ONEWIRE_WRITE_LOW_US      6u
#define ONEWIRE_READ_SAMPLE_US    9u
#define ONEWIRE_BIT_WINDOW_US    60u

static void onewire_drive_low(RoboHubAVROneWire* bus)
{
    if (bus->pin < 8u) {
        PORTB &= (uint8_t)~(1u << bus->pin);
        DDRB |= (uint8_t)(1u << bus->pin);
    } else {
        PORTD &= (uint8_t)~(1u << (bus->pin - 14u));
        DDRD |= (uint8_t)(1u << (bus->pin - 14u));
    }
}

static void onewire_release(RoboHubAVROneWire* bus)
{
    if (bus->pin < 8u) {
        DDRB &= (uint8_t)~(1u << bus->pin);
    } else {
        DDRD &= (uint8_t)~(1u << (bus->pin - 14u));
    }
}

static rh_bool onewire_level(RoboHubAVROneWire* bus)
{
    if (bus->pin < 8u) {
        return (PINB & (uint8_t)(1u << bus->pin)) != 0u ? RH_TRUE : RH_FALSE;
    }
    return (PIND & (uint8_t)(1u << (bus->pin - 14u))) != 0u ? RH_TRUE : RH_FALSE;
}

static void onewire_delay(RoboHubAVROneWire* bus, rh_u32 us)
{
    if (bus->timer != NULL && bus->timer->delay_us != NULL) {
        bus->timer->delay_us(bus->timer, us);
    }
}

static RoboHubError avr_onewire_op_init(RoboHubOneWire* self)
{
    RoboHubAVROneWire* bus = (RoboHubAVROneWire*)self->context;
    if (bus->timer == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    onewire_release(bus);
    return ROBOHUB_OK;
}

static RoboHubError avr_onewire_op_deinit(RoboHubOneWire* self)
{
    (void)self;
    return ROBOHUB_OK;
}

static RoboHubError avr_onewire_op_reset(RoboHubOneWire* self, rh_bool* present)
{
    RoboHubAVROneWire* bus = (RoboHubAVROneWire*)self->context;
    if (present == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    onewire_drive_low(bus);
    onewire_delay(bus, ONEWIRE_RESET_LOW_US);
    onewire_release(bus);
    onewire_delay(bus, ONEWIRE_PRESENCE_US);
    /* The part holds the line low for the rest of its presence pulse if it is
     * there; it is released for the whole of it if it is not. */
    *present = (onewire_level(bus) == RH_FALSE) ? RH_TRUE : RH_FALSE;
    onewire_delay(bus, ONEWIRE_PRESENCE_WINDOW);
    return ROBOHUB_OK;
}

static RoboHubError avr_onewire_op_write_byte(RoboHubOneWire* self, rh_u8 byte)
{
    RoboHubAVROneWire* bus = (RoboHubAVROneWire*)self->context;
    for (rh_u8 bit = 0; bit < 8u; bit++) {
        if (byte & 0x01u) {
            /* A one is a short pulse and a release. */
            onewire_drive_low(bus);
            onewire_delay(bus, 6u);
            onewire_release(bus);
            onewire_delay(bus, ONEWIRE_BIT_WINDOW_US);
        } else {
            /* A zero holds the line low for the whole slot. */
            onewire_drive_low(bus);
            onewire_delay(bus, ONEWIRE_BIT_WINDOW_US);
            onewire_release(bus);
        }
        byte >>= 1;
    }
    return ROBOHUB_OK;
}

static RoboHubError avr_onewire_op_read_byte(RoboHubOneWire* self, rh_u8* byte)
{
    RoboHubAVROneWire* bus = (RoboHubAVROneWire*)self->context;
    if (byte == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rh_u8 value = 0u;
    for (rh_u8 bit = 0; bit < 8u; bit++) {
        onewire_drive_low(bus);
        onewire_delay(bus, ONEWIRE_WRITE_LOW_US);
        onewire_release(bus);
        onewire_delay(bus, ONEWIRE_READ_SAMPLE_US);
        if (onewire_level(bus) == RH_TRUE) {
            value |= (uint8_t)(1u << bit);
        }
        onewire_delay(bus, ONEWIRE_BIT_WINDOW_US - ONEWIRE_READ_SAMPLE_US);
    }
    *byte = value;
    return ROBOHUB_OK;
}

static RoboHubError avr_onewire_op_write(RoboHubOneWire* self, const rh_u8* data, rh_size len)
{
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    for (rh_size i = 0; i < len; i++) {
        (void)avr_onewire_op_write_byte(self, data[i]);
    }
    return ROBOHUB_OK;
}

static RoboHubError avr_onewire_op_read(RoboHubOneWire* self, rh_u8* data, rh_size len)
{
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    for (rh_size i = 0; i < len; i++) {
        (void)avr_onewire_op_read_byte(self, &data[i]);
    }
    return ROBOHUB_OK;
}

RoboHubError avr_onewire_create(RoboHubAVROneWire* bus, rh_name name, rh_u32 pin, RoboHubTimer* timer)
{
    if (bus == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    bus->pin = pin;
    bus->timer = timer;
    bus->interface.name = name;
    bus->interface.context = bus;
    bus->interface.init = avr_onewire_op_init;
    bus->interface.deinit = avr_onewire_op_deinit;
    bus->interface.reset = avr_onewire_op_reset;
    bus->interface.write_byte = avr_onewire_op_write_byte;
    bus->interface.read_byte = avr_onewire_op_read_byte;
    bus->interface.write = avr_onewire_op_write;
    bus->interface.read = avr_onewire_op_read;
    return ROBOHUB_OK;
}

/*
 * avr_onewire.h — 1-Wire interface implementation for AVR parts.
 *
 * A 1-Wire bus is one line, and the protocol is timed: a reset is a long low
 * pulse, a bit is a short pulse and a sample, and the part answers a reset by
 * pulling the line low. Timing it needs a counter that runs while the program
 * waits, which is what this platform's timer is for.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_ONEWIRE_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_ONEWIRE_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/onewire.h"
#include "robohub/hal/timer.h"

typedef struct {
    RoboHubOneWire interface; /* Must remain the first member. */
    rh_u32     pin;
    RoboHubTimer* timer;    /* What the pulse lengths are measured with. */
} RoboHubAVROneWire;

/**
 * @brief Bind a 1-Wire line to a pin.
 *
 * The timer is what the pulse lengths are measured with, and without one the bus
 * cannot be timed; it is the platform's `system_timer`.
 */
RoboHubError avr_onewire_create(RoboHubAVROneWire* bus, rh_name name, rh_u32 pin, RoboHubTimer* timer);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_ONEWIRE_H */

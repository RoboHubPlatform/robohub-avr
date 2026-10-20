/**
 * @file avr_dht.h — DHT interface implementation for AVR parts.
 *
 * A DHT is one line and a timed conversation: the host asks with a long low
 * pulse, the part answers with a short low and a short high, and then forty bits
 * that are timed rather than sent. The timing is what the interface is, and it
 * is measured with the platform's timer.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_DHT_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_DHT_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/dht.h"
#include "robohub/hal/timer.h"

typedef struct {
    RoboHubDHT interface; /* Must remain the first member. */
    rh_u32     pin;
    RoboHubTimer* timer;    /* What the pulse lengths are measured with. */
} RoboHubAVRDHT;

/**
 * @brief Bind a DHT line to a pin.
 *
 * The line is open-drain: the part holds it high, and a low is the host
 * driving it. The timer measures the pulses; without one the frame cannot be
 * read.
 */
RoboHubError avr_dht_create(RoboHubAVRDHT* dht, rh_name name, rh_u32 pin, RoboHubTimer* timer);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_DHT_H */

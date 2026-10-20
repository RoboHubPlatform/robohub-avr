/*
 * avr_timer.h — timer interface implementation for AVR parts.
 *
 * An AVR counts with two timers of different widths, and this platform uses one
 * for each job: the eight-bit one for the millisecond clock, the sixteen-bit one
 * free-running for the microsecond one. A delay is a wait on the counter, so it
 * is exact to the counter's tick and does not lose time to an interrupt that
 * happens while it waits.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_TIMER_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/timer.h"

/* One AVR clock: the interface and nothing else, because the counters are the
 * part's and are shared by everything that asks for the time. */
typedef struct {
    RoboHubTimer interface; /* Must remain the first member. */
} RoboHubAVRTimer;

/* Bind the clock. */
RoboHubError avr_timer_create(RoboHubAVRTimer* timer, rh_name name);

/* The millisecond clock, started by the platform's bring-up. */
void avr_timer_start(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_TIMER_H */

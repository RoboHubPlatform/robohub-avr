/*
 * avr_i2c.h — I2C interface implementation for AVR parts.
 *
 * The ATmega328P has one hardware TWI, on the UNO's A4 and A5. The board carries
 * the pull-ups, and the peripheral drives the lines open-drain.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_I2C_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/i2c.h"

typedef struct {
    RoboHubI2C interface; /* Must remain the first member. */
    rh_u32    speed_hz;
} RoboHubAVRI2C;

/** Bind the TWI peripheral. */
RoboHubError avr_i2c_create(RoboHubAVRI2C* i2c, rh_name name);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_I2C_H */

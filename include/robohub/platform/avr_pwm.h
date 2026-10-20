/*
 * avr_pwm.h — PWM interface implementation for AVR parts.
 *
 * The ATmega328P has three timers and six pins that can be modulated. Two of
 * them belong to the timer this platform counts milliseconds with, so four are
 * left here: the two of the sixteen-bit timer and the two of the eight-bit one.
 *
 * The four are the UNO's D9, D10, D11 and D3, in that order.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_PWM_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/pwm.h"

/** How many channels this platform modulates. */
#define AVR_PWM_CHANNELS 4u

typedef struct {
    RoboHubPWM interface; /* Must remain the first member. */
    rh_u32     pin[AVR_PWM_CHANNELS]; /* The pin behind each channel. */
    rh_u16     top;                   /* The counter's wrap. */
    rh_f32     duty[AVR_PWM_CHANNELS];
    rh_u32     frequency_hz;
    rh_bool    ready;
} RoboHubAVRPWM;

/**
 * @brief Bind the modulator.
 *
 * The channels are the UNO's D9, D10, D11 and D3, in that order.
 */
RoboHubError avr_pwm_create(RoboHubAVRPWM* pwm, rh_name name);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_PWM_H */

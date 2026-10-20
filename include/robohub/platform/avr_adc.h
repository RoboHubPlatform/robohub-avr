/*
 * avr_adc.h — ADC interface implementation for AVR parts.
 *
 * The ATmega328P has a ten-bit converter with six inputs, and this is the
 * contract over it: a channel, and a level from 0 to 1 of full scale.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_ADC_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/adc.h"

/** How many inputs the converter has. */
#define AVR_ADC_CHANNELS 6u
/** How many bits a conversion is. */
#define AVR_ADC_BITS 10u

typedef struct {
    RoboHubADC interface; /* Must remain the first member. */
} RoboHubAVRADC;

/** Bind the converter. */
RoboHubError avr_adc_create(RoboHubAVRADC* adc, rh_name name);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_ADC_H */

/*
 * avr_spi.h — SPI interface implementation for AVR parts.
 *
 * The ATmega328P has one hardware SPI, on the UNO's D13, D12 and D11, with D10
 * as the select.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_SPI_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/spi.h"

typedef struct {
    RoboHubSPI interface; /* Must remain the first member. */
    rh_u32    speed_hz;
} RoboHubAVRSPI;

/** Bind the SPI peripheral. */
RoboHubError avr_spi_create(RoboHubAVRSPI* spi, rh_name name);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_SPI_H */

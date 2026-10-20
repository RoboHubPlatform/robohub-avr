/*
 * avr_platform.h — Platform initialization for AVR parts.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_PLATFORM_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/platform/avr_uart.h"

/**
 * @brief Bring the platform up.
 *
 * Starts the counters and points the console at the USART the board gave it.
 * The board's own peripherals are registered by the board's support package,
 * which is what knows them.
 */
RoboHubError avr_platform_init(RoboHubAVRUART* console_uart);

/**
 * @brief Stop the program where a debugger can find it, and never return.
 *
 * This is what a panic ends in on this platform. The runtime calls it instead
 * of the C library's `exit`, which on a part with 2 KB of RAM means a firmware
 * that carries an exit path it has nowhere to go from.
 *
 * Declared for the platform's own readers; the runtime declares it too, because
 * what a platform is is its own headers and the runtime does not know them.
 */
void rho_avr_halt(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_PLATFORM_H */

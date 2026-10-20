/*
 * avr_console.h — the console on AVR parts.
 *
 * The board's bring-up hands the console the USART it is on, because the speed
 * the console runs at is the board's fact.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_CONSOLE_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "robohub/platform/avr_uart.h"

/* The console writes to this USART from now on. */
void avr_console_attach(RoboHubAVRUART* uart);

/*
 * The runtime's output: the console, by the name the runtime is told to call.
 *
 * The name is the one the platform's CMake exports, so the generated runtime
 * calls this and not whatever it would have guessed.
 */
int rho_avr_write(int fd, const char* data, int len);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_CONSOLE_H */

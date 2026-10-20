/*
 * avr_uart.h — UART interface implementation for AVR parts.
 *
 * The ATmega328P has one hardware USART, on the pins an Arduino calls D0 and
 * D1. This is the contract over it.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_AVR_UART_H
#define INCLUDE_ROBOHUB_PLATFORM_AVR_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/uart.h"

/* One AVR USART: the interface and the speed it was brought up at. */
typedef struct {
    RoboHubUART interface; /* Must remain the first member. */
    rh_u32      baudrate;
} RoboHubAVRUART;

/* Bind the USART. The speed is set when the line is brought up. */
RoboHubError avr_uart_create(RoboHubAVRUART* uart, rh_name name);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_AVR_UART_H */

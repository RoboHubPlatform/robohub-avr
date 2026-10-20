/*
 * bsp.h — Support package for the Arduino UNO R3.
 *
 * The board is an ATmega328P and a set of headers. What this file carries is
 * what the headers mean: which pin is the user LED, which are the serial lines,
 * which carry I2C, and which six can be modulated.
 *
 * The pin numbers here are the board's silkscreen numbers, D0 to D13 and A0 to
 * A5, and they are the numbers a person reads off the board. The part's own pin
 * numbers — A0 is pin 14 on the package — appear only where the part needs
 * them.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_UNO_R3_BSP_BSP_H
#define BOARDS_UNO_R3_BSP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "platform.h"

/* -------------------------------------------------------------------------- */
/*  Pins                                                                      */
/* -------------------------------------------------------------------------- */

/* The user LED, on D13. It is an output, and it is on when the pin is high. */
#define RH_UNO_R3_LED_PIN        13u
/* The serial lines, on the USB header and the digital pins: D0 receives and D1
 * transmits. */
#define RH_UNO_R3_UART_RX_PIN     0u
#define RH_UNO_R3_UART_TX_PIN     1u
/* I2C, on the analog header: A4 is the data line and A5 the clock. */
#define RH_UNO_R3_I2C_SDA_PIN    18u
#define RH_UNO_R3_I2C_SCL_PIN    19u
/* SPI, on the digital header: D11 is the output, D12 the input and D13 the
 * clock, with D10 as the select. */
#define RH_UNO_R3_SPI_MOSI_PIN   11u
#define RH_UNO_R3_SPI_MISO_PIN   12u
#define RH_UNO_R3_SPI_SCK_PIN    13u
#define RH_UNO_R3_SPI_SS_PIN     10u
/* A 1-Wire bus and a DHT each want a pin of their own; both are open-drain
 * lines with a pull-up, and D2 and D4 are free. */
#define RH_UNO_R3_ONEWIRE_PIN     2u
#define RH_UNO_R3_DHT_PIN         4u

/* The six pins that can be modulated: D3, D5, D6, D9, D10 and D11. Two of them
 * are the SPI lines and one is the select, so a board that uses both at once
 * has to choose — which is a fact about the part and not a limit of this
 * platform. */
#define RH_UNO_R3_PWM_PINS       3u, 5u, 6u, 9u, 10u, 11u
#define RH_UNO_R3_PWM_CHANNELS    6u

/* The six analog inputs, A0 to A5, which are the part's pins 14 to 19. */
#define RH_UNO_R3_ADC_PINS       14u, 15u, 16u, 17u, 18u, 19u
#define RH_UNO_R3_ADC_CHANNELS    6u

/* The speed the console runs at, and the one the serial link to a gateway would
 * use. */
#define RH_UNO_R3_CONSOLE_BAUD 115200u

/* -------------------------------------------------------------------------- */
/*  Board functions                                                           */
/* -------------------------------------------------------------------------- */

/** The board's own bring-up: the console, and the pin the LED is on. */
void bsp_init(void);

/** Register the board's peripherals under the names a program asks for. */
void bsp_register_hals(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_UNO_R3_BSP_BSP_H */

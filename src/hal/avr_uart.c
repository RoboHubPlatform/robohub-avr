/*
 * avr_uart.c — UART interface implementation for AVR parts.
 *
 * The USART is brought up with the double-speed bit, which buys the resolution
 * a 16 MHz part needs to hold a standard speed exactly: at 9600 the divisor
 * works out to a whole number, and at 115200 it works out to a number that is
 * within a fraction of a percent.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "avr_uart.h"

#include <avr/io.h>

#include <stddef.h>

static RoboHubError avr_uart_op_init(RoboHubUART* self, rh_u32 baudrate)
{
    RoboHubAVRUART* uart = (RoboHubAVRUART*)self->context;
    if (baudrate == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    uart->baudrate = baudrate;

    uint16_t divisor = (uint16_t)((F_CPU / 8u + baudrate / 2u) / baudrate - 1u);
    if (divisor == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    UCSR0A = (uint8_t)(1u << U2X0);   /* double speed, so the divisor is finer */
    UCSR0B = (uint8_t)((1u << TXEN0) | (1u << RXEN0));
    UCSR0C = (uint8_t)((1u << UCSZ01) | (1u << UCSZ00)); /* eight bits, no parity */
    UBRR0 = divisor;
    return ROBOHUB_OK;
}

static RoboHubError avr_uart_op_write(RoboHubUART* self, const rh_u8* data, rh_size len)
{
    (void)self;
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    for (rh_size i = 0; i < len; i++) {
        while (!(UCSR0A & (uint8_t)(1u << UDRE0))) {
            /* The register is empty; wait for it. */
        }
        UDR0 = data[i];
    }
    /* The last byte is still in the shift register: a caller that writes and
     * then pulls the line down would cut it short. */
    while (!(UCSR0A & (uint8_t)(1u << TXC0))) {
    }
    return ROBOHUB_OK;
}

static RoboHubError avr_uart_op_read(RoboHubUART* self, rh_u8* data, rh_size len)
{
    (void)self;
    if (data == NULL && len != 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    for (rh_size i = 0; i < len; i++) {
        while (!(UCSR0A & (uint8_t)(1u << RXC0))) {
            /* Nothing has arrived; wait for it. */
        }
        data[i] = UDR0;
    }
    return ROBOHUB_OK;
}

static rh_size avr_uart_op_read_available(RoboHubUART* self, rh_u8* data, rh_size max_len)
{
    (void)self;
    if (data == NULL || max_len == 0u) {
        return 0u;
    }
    if (!(UCSR0A & (uint8_t)(1u << RXC0))) {
        return 0u;
    }
    data[0] = UDR0;
    return 1u;
}

RoboHubError avr_uart_create(RoboHubAVRUART* uart, rh_name name)
{
    if (uart == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    uart->baudrate = 0u;
    uart->interface.name = name;
    uart->interface.context = uart;
    uart->interface.init = avr_uart_op_init;
    uart->interface.write = avr_uart_op_write;
    uart->interface.read = avr_uart_op_read;
    uart->interface.read_available = avr_uart_op_read_available;
    return ROBOHUB_OK;
}

/*
 * avr_console.c — the console contract on AVR parts.
 *
 * The console is the part's USART, and a program that prints through this
 * contract and a program that prints through the C library reach the same place:
 * both go out of the same register, and the C library's stdout is pointed at it
 * by the board's bring-up.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "robohub/hal/console.h"
#include "robohub/platform/avr_uart.h"

#include <stdio.h>
#include <string.h>

RoboHubConsole robohub_console;

/* The console's USART. The board creates it and hands it over, because which
 * speed the console runs at is the board's fact and not the console's. */
static RoboHubAVRUART* avr_console_uart;

void avr_console_attach(RoboHubAVRUART* uart)
{
    avr_console_uart = uart;
}

RoboHubConsoleError robohub_console_init(void)
{
    if (robohub_console.ready) {
        return ROBOHUB_CONSOLE_OK;
    }
    robohub_console.ready = 1u;
    robohub_console.written = 0u;
    return ROBOHUB_CONSOLE_OK;
}

/*
 * The runtime's output, as the platform's console.
 *
 * A Rho program prints through the C library, and the C library's stdout goes
 * wherever the platform says. This is that: the console's USART, which is where
 * the board's own console goes, so a `print` in a program and a write through the
 * console contract reach the same place.
 */
int rho_avr_write(int fd, const char* data, int len)
{
    (void)fd;
    if (avr_console_uart == NULL || data == NULL || len <= 0) {
        return -1;
    }
    if (avr_console_uart->interface.write(&avr_console_uart->interface,
                                          (const rh_u8*)data, (rh_size)len) != ROBOHUB_OK) {
        return -1;
    }
    return len;
}

RoboHubConsoleError robohub_console_write(const char* data, size_t len)
{
    if (data == NULL && len != 0u) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    if (!robohub_console.ready) {
        return ROBOHUB_CONSOLE_ERR_STATE;
    }
    if (avr_console_uart == NULL) {
        return ROBOHUB_CONSOLE_ERR_UNSUPPORTED;
    }
    if (avr_console_uart->interface.write(&avr_console_uart->interface,
                                          (const rh_u8*)data, (rh_size)len) != ROBOHUB_OK) {
        return ROBOHUB_CONSOLE_ERR_TIMEOUT;
    }
    robohub_console.written += (uint64_t)len;
    return ROBOHUB_CONSOLE_OK;
}

RoboHubConsoleError robohub_console_puts(const char* text)
{
    if (text == NULL) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    return robohub_console_write(text, strlen(text));
}

RoboHubConsoleError robohub_console_write_raw(const uint8_t* data, size_t len)
{
    return robohub_console_write((const char*)data, len);
}

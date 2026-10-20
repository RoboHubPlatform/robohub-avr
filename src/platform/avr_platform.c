/*
 * avr_platform.c — Platform initialization for AVR parts.
 *
 * An AVR boots with its registers at their reset values and nothing else running:
 * there is no clock tree to choose, no pin mux to program, no RAM to copy. What
 * there is to do is start the counters — the millisecond clock every driver's
 * delay is counted in — and point the console at the USART.
 *
 * The board's own facts are not here. Which pin the user LED is on, which pins
 * the buses are on: those belong to the board, and the board's support package
 * registers them, so a program asks for a name and never for a pin.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "robohub/platform/avr_platform.h"
#include "robohub/platform/avr_timer.h"
#include "robohub/platform/avr_console.h"
#include "robohub/platform/avr_uart.h"
#include "robohub/hal/console.h"

#if defined(__AVR__)
#include <avr/interrupt.h>
#include <avr/io.h>
#else
#include <stdlib.h>
#endif

RoboHubError avr_platform_init(RoboHubAVRUART* console_uart)
{
    /* The counters first: everything else may want to time something. */
    avr_timer_start();

    if (console_uart != NULL) {
        avr_console_attach(console_uart);
        (void)robohub_console_init();
    }
    return ROBOHUB_OK;
}

void rho_avr_halt(void)
{
#if defined(__AVR__)
    /* Interrupts off, so what is read after the stop is what led to it: a timer
     * that went on ticking would have moved on by the time anybody looked.
     *
     * `break` is the AVR's debug break. A part with a debugger attached stops
     * here with the call stack intact; one without treats it as a no-op, which
     * is why the loop after it is what actually parks the program — the core
     * stays where the failure is rather than running off into whatever the
     * program's next bytes happen to be. */
    cli();
    for (;;) {
        __asm__ volatile("break");
    }
#else
    /* Not a part: this is the platform compiled for a host, which is how the
     * half of it that has nothing to do with the chip is tested. A host stops
     * the way a host stops, and the reason is the caller's to report. */
    abort();
#endif
}

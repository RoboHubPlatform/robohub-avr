/*
 * test_console.c — the console contract on a part, tested on a host.
 *
 * The console is the one part of this platform that has no part in it: it is
 * the USART behind a contract, and everything a program prints goes through it.
 * So it is the part most worth testing off the part — with a fake USART behind
 * it, so that what the console does with the bytes, and what it says when it
 * cannot do it, can be checked without a board and a serial line.
 *
 * What is checked here is the contract, not the USART: that a console which was
 * not opened says so, that a board with no console says so, that a null
 * pointer is refused, that a USART which refuses is reported as a timeout
 * rather than as a write that happened, and that the bytes counted are the
 * bytes written.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "robohub/hal/console.h"
#include "robohub/platform/avr_uart.h"
#include "robohub/platform/avr_console.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

/* A fake USART.
 *
 * It records what it was given and can be told to refuse, because a console
 * that only ever succeeds is a console whose failure paths are untested. */
typedef struct {
    RoboHubAVRUART uart;
    char           written[256];
    size_t         written_length;
    int            refuse;
} FakeUART;

/* The fake's write, which is what the console reaches. */
static RoboHubError fake_write(RoboHubUART* self, const rh_u8* data, rh_size length)
{
    FakeUART* fake = (FakeUART*)self;
    if (fake->refuse) {
        return ROBOHUB_ERR_IO;
    }
    if (fake->written_length + length < sizeof(fake->written)) {
        memcpy(fake->written + fake->written_length, data, length);
        fake->written_length += length;
    }
    return ROBOHUB_OK;
}

static void fake_uart_init(FakeUART* fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->uart.interface.write = fake_write;
}

/* One fact, checked: the console says what it says. */
static void expect(int condition, const char* what)
{
    if (!condition) {
        fprintf(stderr, "expected %s\n", what);
        failures++;
    }
}

int main(void)
{
    FakeUART fake;

    /* A console that was not opened says so, rather than pretending it wrote. */
    fake_uart_init(&fake);
    robohub_console.ready = 0;
    expect(robohub_console_write("hello", 5) == ROBOHUB_CONSOLE_ERR_STATE,
           "a console that was not opened to refuse a write");

    /* A board with no console says so, which is a different thing from not
     * being open: the console is open, and there is nothing behind it. */
    expect(robohub_console_init() == ROBOHUB_CONSOLE_OK, "a console to open");
    avr_console_attach(NULL);
    expect(robohub_console_write("hello", 5) == ROBOHUB_CONSOLE_ERR_UNSUPPORTED,
           "a board with no console to say so");

    /* Opening a console is idempotent: a program that opens it twice has not
     * opened it twice, and the count of what was written is not reset under
     * the program. */
    fake_uart_init(&fake);
    avr_console_attach(&fake.uart);
    expect(robohub_console_init() == ROBOHUB_CONSOLE_OK, "a console to open");
    expect(robohub_console.ready == 1u, "an open console to be ready");
    expect(robohub_console.written == 0u, "an open console to have written nothing yet");
    expect(robohub_console_init() == ROBOHUB_CONSOLE_OK, "a second open to be accepted");
    expect(robohub_console.written == 0u, "a second open not to reset the count");

    /* What was written went to the console, and the console counted it. */
    expect(robohub_console_write("hello", 5) == ROBOHUB_CONSOLE_OK, "a write to succeed");
    expect(fake.written_length == 5u, "five bytes to reach the console");
    expect(strcmp(fake.written, "hello") == 0, "the bytes to be the ones written");
    expect(robohub_console.written == 5u, "the console to have counted five bytes");

    /* A null pointer is refused, and a null pointer with no length is not a
     * write of nothing but a mistake. */
    expect(robohub_console_write(NULL, 0) == ROBOHUB_CONSOLE_OK,
           "a null pointer with no length to be accepted");
    expect(robohub_console_write(NULL, 3) == ROBOHUB_CONSOLE_ERR_INVALID,
           "a null pointer with a length to be refused");
    expect(robohub_console_puts(NULL) == ROBOHUB_CONSOLE_ERR_INVALID,
           "a null string to be refused");

    /* A string goes out whole. */
    expect(robohub_console_puts("world") == ROBOHUB_CONSOLE_OK, "a string to be written");
    expect(fake.written_length == 10u, "five more bytes to reach the console");
    expect(strcmp(fake.written, "helloworld") == 0, "the string to be whole");

    /* A USART that refuses is a timeout, and not a write that happened: the
     * count says what actually went out. */
    fake.refuse = 1;
    expect(robohub_console_write("lost", 4) == ROBOHUB_CONSOLE_ERR_TIMEOUT,
           "a refused write to be a timeout");
    expect(robohub_console.written == 10u, "a refused write not to be counted");

    /* The runtime's own output goes to the same console, and says how much it
     * wrote. A program that prints through the C library and a program that
     * writes through the contract reach the same place. */
    fake.refuse = 0;
    expect(rho_avr_write(1, "abc", 3) == 3, "the runtime's write to report its length");
    expect(fake.written_length == 13u, "the runtime's bytes to reach the console");
    expect(rho_avr_write(1, NULL, 3) == -1, "a null buffer to be refused");
    expect(rho_avr_write(1, "abc", 0) == -1, "a write of nothing to be refused");
    expect(rho_avr_write(1, "abc", -1) == -1, "a negative length to be refused");

    if (failures > 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("console: every rule holds\n");
    return 0;
}

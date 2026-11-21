# Changelog

All notable changes to this module are recorded here, one section per release.
A module's description says what it is and does not change between releases; this
says what changed, which is what tells two versions of one module apart. The
section for the version being published travels with it, so a registry shows it
against that version.

## [0.1.0] - 2026-10-20

The 8-bit AVR platform for the RoboHub Platform: the interfaces declared by
`robohub-common`, implemented on an ATmega328P, with a board support package for
the Arduino UNO R3.

Two examples. `examples/c/hello_world` is the smallest program that runs on
real hardware — it blinks the user LED and writes a line to the console every
second, and it is C from `#include` to `main`. `examples/rho/hello_world` is
that program in Rho: written against the contracts and nothing else, with the
foreign calls derived from the contract headers rather than written out beside
them, so the same program builds for any board the platform supports.

The board's own manifest now says what the toolchain's schema asks of it: a
connector is named and what it carries is said of it, a programmer is a map, and
the peripherals are a list. A manifest that does not match is not read at all,
and a board whose manifest is not read is a board that does not exist.

The platform says how a program on it stops. A panic ends in the part's own
debug break rather than in the C library's `exit`, which on a part with 2 KB of
RAM is a firmware carrying an exit path it has nowhere to go from.

The part says that the toolchain brings its own linker script, so none is
written for it: `avr-gcc` knows where an AVR's vector table belongs and how its
runtime expects to be reached.

The host test that the build has been asking for is here, and the vendored
platform's tests are no longer registered in a build that does not build them.

The tests are registered in a registry of their own, so that the orchestrator can
say what they are, run one of them by its name, and say whether it is run at
all: `robohub test list`, `robohub test run`, `robohub test enable` and
`disable`. A test that is not enabled is still a test — it is there, and it is
not run.

The platform's tests are in folders now, as the unit tests and the smoke tests
are two different things: `tests/unit/` holds the tests of the chip-independent
half — the error codes every layer agrees on, and the console, which is the one
part of the platform that has no part in it — and `tests/smoke/` holds the
examples, built for the board the platform carries. A manifest says which is
which, and the orchestrator runs either on its own.

The console's state is declared in its header now, because a test that cannot
see whether a console is open cannot check that a console which was not opened
says so.

Which serial line the board is on is no longer written into the board's spec.
The flash command reads it from the environment — `AVRDUDE_PORT` — and the
value in the spec is what the machine falls back to, so a board on another line
is flashed without the platform being changed.

## [0.2.0] - 2026-11-14

The board key is kept in EEPROM below the nonce counter. AVR has no production mode. Host tests build without avr-libc.

## [0.2.1] - 2026-11-15

Installing the AVR compiler installs its C library too (`apt: gcc-avr avr-libc`): Debian ships avr-libc apart from gcc-avr, so a machine set up from this manifest alone — a fresh CI runner — failed on `string.h`.

## [0.2.2] - 2026-11-19

Security: a board's nonces no longer recur after a chip erase. A chip erase with
the EESAVE fuse unprogrammed (an ISP programmer, burning a bootloader) put the
EEPROM boot counter back to all ones while the board's compiled-in key stayed the
same, so the board said the nonces of its first boots again; someone who had
recorded the gateway's session answer to one of them could send it again, and
the board would take the recorded session's keys, restart its counters and accept
that session's recorded commands. A counter that reads as erased is now seeded
once from the watchdog's jitter against the CPU clock (about a second, interrupts
off) instead of advanced from zero, and a source that does not vary, or a part
with WDTON programmed, gives no nonce and no session rather than a guessable one
(ADR-0002, amending ADR-0001; AVR-0009). A counter that is not erased counts on as
before. The uno-r3 flash command passes avrdude `-D`, so `rho flash` never asks
for a chip erase and EEPROM survives it (AVR-0010; uno-r3 0.1.1); the docs say how to program
EESAVE before using an ISP programmer. `tests/unit/test_nonce.c` simulates reboots
and chip erases over a fake EEPROM and checks no nonce is said twice.

## [0.3.0] - 2026-11-20

The health contracts on the ATmega328P (AVR-0011, robohub-common 1.3.0): the watchdog in interrupt-and-reset mode — its interrupt is the early warning that writes the record, and the reset follows one period later, so a period is two halves (8000 ms: a 4 s warning, then the reset); stoppable unless WDTON is programmed. MCUSR is saved and cleared in `.init3`, with the watchdog switched off there so a watchdog reset does not repeat before `main`; when the UNO's optiboot has cleared it, its copy in r2 is taken in `.init0`. Optiboot passes every reset-pin press and upload through its own watchdog, so a watchdog flag is reported as `watchdog` only when this program's early warning marked it, and as `unknown` otherwise. The fault record and boot count live in 32 bytes of `.noinit` RAM.

## [0.3.1] - 2026-11-21

A health check for the board (AVR-0012, robohub ADR-0025). The UNO R3 runs no core, so `examples/c/health_check` calls the contracts itself, across three boots:

- key A is stored in EEPROM, and the watchdog is started and never fed;
- its early warning has written the record, key A has held across the reset, and key B is stored before the watchdog is starved again;
- key B has held, and the watchdog is fed for 20 s.

The check prints `health-check: passed`, and `health_check.py` builds it, flashes it and reads the console. It is built with the tests, and the run on a board is declared skipped until one is on the bench. The check's keys have their own scope and epoch 0, so no session uses them, but they replace a key the board was rotated to.

